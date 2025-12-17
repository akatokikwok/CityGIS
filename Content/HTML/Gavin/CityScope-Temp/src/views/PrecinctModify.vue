<script setup lang="ts">
import { onMounted, onUnmounted, reactive, ref } from 'vue';
import { useBaiduMap } from '@/composable/useBaiduMap'; // 假设路径一致
import * as turf from '@turf/turf';
import { 
    addPrecinct, 
    getPrecinctByCode, 
    updatePrecinct, 
    getSubdistrictGeo // 之前定义的获取街道围栏接口，这里也可能用到
} from '@/api/geoApi'; 

// 使用封装好的 hook
const { mapInstance, initMap, destroyMap } = useBaiduMap();
const MAP_CONTAINER_ID = 'visual-map-edit-container';

// --- 变量定义 ---
let drawScene: any = null;
let polygonDraw: any = null;
let polygonEdit: any = null;
let currentPolygon: any = null; // 当前正在编辑的主多边形
let referenceLayers: any[] = []; // 参考图层
// [新增] 用于缓存参考区域的原始 GeoJSON 数据，key为code，value为{ type, geometry }
const referenceDataCache = new Map<string, { type: 'street' | 'block', geometry: any }>();
let centroidMarker: any = null; // 中心点 Marker

const isSubmitting = ref(false);
const isSearching = ref(false);
const isEditing = ref(false); // 是否处于编辑模式
const isDrawing = ref(false); // 是否正在绘制模式 (编辑页通常直接加载，不太用重新绘制，但也保留功能)
const hasPolygon = ref(false); // 是否存在区域

// 搜索相关
const searchCode = ref('');
const currentEditCode = ref(''); // 记录当前正在编辑的 code，作为提交时的依据

// 参考图层输入框
const refStreetCode = ref(''); 
const refBlockCode = ref('');

// 表单数据
const formData = reactive({
    name: '',
    districtCode: '',
    properties: {} //以此来存储接口返回的 extra properties
});

// ======== 1. 核心：加载与回显逻辑 ========

const handleSearch = async () => {
    if (!searchCode.value) {
        alert('请输入板块 Code');
        return;
    }
    
    if (hasPolygon.value) {
        if (!confirm('当前地图上已有正在编辑的数据，重新搜索将覆盖，是否继续？')) {
            return;
        }
    }

    try {
        isSearching.value = true;
        // 清理现有画布
        clearAll();

        const res: any = await getPrecinctByCode(searchCode.value);
        console.log('获取板块数据:', res);
        const data = res;
        
        // 1. 回填表单数据
        formData.name = data.name;
        formData.districtCode = data.district_code;
        formData.properties = data.properties || {};
        currentEditCode.value = data.code; // 锁定正在编辑的 ID/Code

        // 2. 回显多边形 (GeoJSON -> BMap Polygon)
        if (data.geometry) {
            renderPolygonFromGeoJSON(data.geometry);
        }
        
        // 3. 自动定位视野
        if (currentPolygon) {
            mapInstance.value.setViewport(currentPolygon.getPath());
        }

    } catch (error) {
        console.error(error);
        alert('获取数据发生异常');
    } finally {
        isSearching.value = false;
    }
};

// 将后端返回的 GeoJSON 渲染为百度地图覆盖物
const renderPolygonFromGeoJSON = (geometry: any) => {
    const BMapClass = window.BMapGL || window.BMap;
    let coords = [];

    // 处理 MultiPolygon (通常数据库存的是 MultiPolygon)
    // 结构: [[[[lng, lat], ...]]]
    if (geometry.type === 'MultiPolygon') {
        // 取第一个多边形的第一个环（外环）
        // 如果有多个分离的块或镂空，逻辑会更复杂，这里按单一大区域处理
        if (geometry.coordinates && geometry.coordinates.length > 0) {
            coords = geometry.coordinates[0][0];
        }
    } else if (geometry.type === 'Polygon') {
        // 结构: [[[lng, lat], ...]]
        coords = geometry.coordinates[0];
    }

    if (coords.length === 0) return;

    // 转换坐标点
    const path = coords.map((c: any) => new BMapClass.Point(c[0], c[1]));
    
    // 百度地图不需要首尾闭合，GeoJSON 是闭合的，如果重叠可以去掉最后一个点
    const start = path[0];
    const end = path[path.length - 1];
    if (start.equals(end)) {
        path.pop();
    }

    // 创建覆盖物
    const polygon = new BMapClass.Polygon(path, {
        strokeColor: '#1890ff',
        fillColor: '#1890ff',
        strokeWeight: 2,
        strokeOpacity: 0.8,
        fillOpacity: 0.3
    });

    mapInstance.value.addOverlay(polygon);
    currentPolygon = polygon;
    hasPolygon.value = true;
    
    // 绑定编辑时的更新事件
    polygon.addEventListener('lineupdate', updateCentroid);

    // 计算一次中心点
    updateCentroid();
};

// 清理当前编辑状态 (搜索新数据前调用)
const clearAll = () => {
    clearCurrentPolygon();
    currentEditCode.value = '';
    formData.name = '';
    formData.districtCode = '';
    formData.properties = {};
};

// ======== 2. 交互逻辑 (绘制/编辑/删除) ========

// 切换绘制模式 (如果现有数据不对，用户想重画)
const toggleDrawMode = () => {
    if (hasPolygon.value) {
        if(confirm('确定要清除当前区域重新绘制吗？')) {
            clearCurrentPolygon();
            startDrawing();
        }
    } else {
        startDrawing();
    }
};

const startDrawing = () => {
    if (polygonDraw && !polygonDraw.isOpen) {
        isDrawing.value = true;
        polygonDraw.open();
    }
};

const handleDrawComplete = (overlay: any) => {
    polygonDraw.close();
    isDrawing.value = false;
    currentPolygon = overlay;
    hasPolygon.value = true;
    updateCentroid();
    overlay.addEventListener('lineupdate', updateCentroid);
};

// 清除多边形
const clearCurrentPolygon = () => {
    if (isEditing.value && polygonEdit) {
        polygonEdit.close();
        isEditing.value = false;
    }
    if (drawScene && currentPolygon) {
        // 注意：如果是 BMapDraw 管理的，可能需要 drawScene.removeOverlay，
        // 但如果是手动 addOverlay 的 (回显时)，则用 map.removeOverlay
        mapInstance.value.removeOverlay(currentPolygon); 
        currentPolygon = null;
    }
    if (centroidMarker) {
        mapInstance.value.removeOverlay(centroidMarker);
        centroidMarker = null;
    }
    hasPolygon.value = false;
};

// 切换编辑模式
const toggleEditMode = () => {
    if (!currentPolygon) return;
    if (isEditing.value) {
        polygonEdit.close();
        isEditing.value = false;
        updateCentroid();
    } else {
        polygonEdit.open(currentPolygon);
        isEditing.value = true;
    }
};

// 更新中心点 (同添加页面)
const updateCentroid = () => {
    if (!currentPolygon) return;
    const path = currentPolygon.getPath();
    if (!path || path.length < 3) return;

    const coords = path.map((p: any) => [p.lng, p.lat]);
    // 闭合
    if (coords[0][0] !== coords[coords.length - 1][0] || coords[0][1] !== coords[coords.length - 1][1]) {
        coords.push(coords[0]);
    }

    try {
        const polygonGeo = turf.polygon([coords]);
        const center = turf.centerOfMass(polygonGeo);
        const [lng, lat] = center.geometry.coordinates;
        const BMapClass = window.BMapGL || window.BMap;
        const point = new BMapClass.Point(lng, lat);

        if (centroidMarker) {
            centroidMarker.setPosition(point);
        } else {
            const marker = new BMapClass.Marker(point, { title: '板块中心点' });
            mapInstance.value.addOverlay(marker);
            centroidMarker = marker;
        }
    } catch (e) {
        console.error(e);
    }
};


// ======== 3. 差集/交集计算逻辑 ========

// --- 1. 请求后端数据并显示参考区域 ---
// type: 'street' | 'block' 用来区分请求哪个接口
const fetchReferenceData = async (type: 'street' | 'block') => {
    const code = type === 'street' ? refStreetCode.value : refBlockCode.value;
    if (!code) {
        alert('请输入编码');
        return;
    }

    // 构造 URL
    const url = type === 'street' 
        ? `http://localhost:3000/subdistrict-geo?code=${code}`
        : `http://localhost:3000/precinct-geo?code=${code}`;

    try {
        const response = await fetch(url);
        const resJson = await response.json();

        // 处理 404 或其他错误
        if (resJson.code !== 200) {
            alert(resJson.message || '获取数据失败');
            return;
        }

        // 获取成功，解析 Geometry
        const geometry = resJson.data.geometry;
        if (geometry) {
            // [新增] 存入缓存，供反向更新使用
            referenceDataCache.set(code, { type, geometry });
            
            drawReferencePolygon(geometry);
        }
        refStreetCode.value = '';
        refBlockCode.value = '';
    } catch (error) {
        console.error('请求参考数据出错', error);
        alert('网络请求失败，请检查后端服务是否启动');
    }
};

// --- 2. 将 GeoJSON 绘制到地图上 (作为参考) ---
const drawReferencePolygon = (geometry: any) => {
    if (!mapInstance.value) return;

    const BMapClass = window.BMapGL || window.BMap;
    
    let polygonsToDraw = [];
    
    if (geometry.type === 'MultiPolygon') {
        polygonsToDraw = geometry.coordinates;
    } else if (geometry.type === 'Polygon') {
        polygonsToDraw = [geometry.coordinates];
    }

    polygonsToDraw.forEach((polyCoords: any) => {
        // polyCoords[0] 通常是外圈
        const outerRing = polyCoords[0]; 
        
        const points = outerRing.map((coord: any) => new BMapClass.Point(coord[0], coord[1]));
        
        const polygonOverlay = new BMapClass.Polygon(points, {
            strokeColor: '#52c41a', // 绿色边框表示参考
            fillColor: '#52c41a',
            strokeWeight: 2,
            strokeOpacity: 0.8,
            fillOpacity: 0.15,     // 淡淡的填充
            strokeStyle: 'dashed'  // 虚线
        });
        
        mapInstance.value.addOverlay(polygonOverlay);
        referenceLayers.push(polygonOverlay); // 存入数组以便后续清除
    });
};

// --- 3. 清空参考区域 ---
const clearReferenceLayers = () => {
    if (!mapInstance.value) return;
    referenceLayers.forEach(layer => {
        mapInstance.value.removeOverlay(layer);
    });
    referenceLayers = [];
    
    // [新增] 清空缓存
    referenceDataCache.clear();
};

// --- [新增] 反向差集计算并更新数据库 ---
const calculateReverseDifferenceAndUpdate = async () => {
    if (!hasPolygon.value || !currentPolygon) {
        alert('请先绘制/加载本地区域');
        return;
    }
    if (referenceDataCache.size === 0) {
        alert('当前没有参考区域数据，无法进行更新');
        return;
    }

    const confirmUpdate = confirm('此操作将直接修改参考区域的数据库数据（裁剪重叠部分），确定要执行吗？');
    if (!confirmUpdate) return;

    try {
        isSubmitting.value = true;

        // 1. 准备“刀具”：当前编辑的区域
        const currentPath = currentPolygon.getPath();
        const currentCoords = currentPath.map((p: any) => [p.lng, p.lat]);
        // 闭合处理
        if (currentCoords.length > 0 && (currentCoords[0][0] !== currentCoords[currentCoords.length - 1][0])) {
            currentCoords.push(currentCoords[0]);
        }
        const cutterPoly = turf.polygon([currentCoords]);

        // 2. 遍历缓存的参考数据
        for (const [code, refData] of referenceDataCache) {
            const { type, geometry: originalGeo } = refData;
            
            // GeoJSON 转 Turf 对象
            let refEntity: any;
            if (originalGeo.type === 'Polygon') {
                refEntity = turf.polygon(originalGeo.coordinates);
            } else if (originalGeo.type === 'MultiPolygon') {
                refEntity = turf.multiPolygon(originalGeo.coordinates);
            } else {
                continue; 
            }

            // 3. 执行差集运算：参考区域 - 当前区域
            const diffResult = turf.difference(turf.featureCollection([refEntity, cutterPoly]));

            if (!diffResult) {
                console.warn(`参考区域 ${code} 被完全移除，跳过更新或需人工介入`);
                continue;
            }

            // 4. 格式化数据准备提交 (转为 MultiPolygon)
            let finalGeometry = diffResult.geometry;
            if (finalGeometry.type === 'Polygon') {
                finalGeometry = {
                    type: 'MultiPolygon',
                    coordinates: [finalGeometry.coordinates]
                };
            }

            // 计算新的中心点
            const newCenter = turf.centerOfMass(diffResult);
            const centerPoint = newCenter.geometry;

            // 5. 调用更新 API
            const apiUrl = type === 'street' 
                ? 'http://localhost:3000/subdistrict/update-inner' 
                : 'http://localhost:3000/precinct/update-inner';
            
            const payload = {
                code: code,
                geometry: finalGeometry,
                center_point: centerPoint
            };

            await fetch(apiUrl, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(payload)
            }).then(res => res.json()).then(res => {
                if(res.code === 200 || res.success) {
                    console.log(`区域 ${code} 更新成功`);
                } else {
                    console.error(`区域 ${code} 更新失败:`, res);
                }
            });
        }

        alert('反向差集更新操作完成，请清空并重新加载参考区域查看最新形状。');
        clearReferenceLayers(); // 自动清空

    } catch (error) {
        console.error('反向差集更新失败', error);
        alert('更新过程中发生错误，请查看控制台');
    } finally {
        isSubmitting.value = false;
    }
};

// --- 4. [核心] 取差集计算 ---
const calculateDifference = () => {
    if (!hasPolygon.value || !currentPolygon) {
        alert('请先绘制本地区域');
        return;
    }
    if (referenceLayers.length === 0) {
        alert('当前没有参考区域，无法取差集');
        return;
    }

    try {
        // 1. 将当前绘制的区域转为 Turf Polygon
        const currentPath = currentPolygon.getPath();
        const currentCoords = currentPath.map((p: any) => [p.lng, p.lat]);
        // 闭合
        if (currentCoords.length > 0 && (currentCoords[0][0] !== currentCoords[currentCoords.length-1][0])) {
            currentCoords.push(currentCoords[0]);
        }
        let mainPoly = turf.polygon([currentCoords]);

        // 2. 遍历所有参考区域，依次进行差集运算 (Main - Ref1 - Ref2 ...)
        referenceLayers.forEach(layer => {
            const path = layer.getPath();
            const coords = path.map((p: any) => [p.lng, p.lat]);
            if (coords.length > 0 && (coords[0][0] !== coords[coords.length-1][0])) {
                coords.push(coords[0]);
            }
            const refPoly = turf.polygon([coords]);
            
            // 执行差集运算
            const diff = turf.difference(turf.featureCollection([mainPoly, refPoly])); 
            
            if (diff) {
                mainPoly = diff; // 更新主区域为差集后的结果
            } else {
                alert('警告：区域被完全减去！');
                mainPoly = null;
                return;
            }
        });

        if (!mainPoly) return;

        // 3. 将计算后的新形状重新绘制到地图上
        updatePolygonShape(mainPoly);

    } catch (error) {
        console.error('差集计算失败', error);
        alert('差集计算出错，可能是几何图形过于复杂');
    }
};

// --- 5. [核心] 取交集计算 ---
const calculateIntersection = () => {
    if (!hasPolygon.value || !currentPolygon) {
        alert('请先绘制本地区域');
        return;
    }
    if (referenceLayers.length === 0) {
        alert('当前没有参考区域，无法取交集');
        return;
    }

    try {
        // 1. 将当前绘制的区域转为 Turf Polygon (主区域)
        const currentPath = currentPolygon.getPath();
        const currentCoords = currentPath.map((p: any) => [p.lng, p.lat]);
        // 闭合
        if (currentCoords.length > 0 && (currentCoords[0][0] !== currentCoords[currentCoords.length-1][0])) {
            currentCoords.push(currentCoords[0]);
        }
        const mainPoly = turf.polygon([currentCoords]);

        // 用于存储所有产生的交集部分
        let accumulatedIntersection: any = null;
        let hasIntersection = false;

        // 2. 遍历所有参考区域
        referenceLayers.forEach(layer => {
            const path = layer.getPath();
            const coords = path.map((p: any) => [p.lng, p.lat]);
            if (coords.length > 0 && (coords[0][0] !== coords[coords.length-1][0])) {
                coords.push(coords[0]);
            }
            const refPoly = turf.polygon([coords]);

            // 取交集
            const intersectPart = turf.intersect(turf.featureCollection([mainPoly, refPoly]));

            if (intersectPart) {
                hasIntersection = true;
                if (!accumulatedIntersection) {
                    accumulatedIntersection = intersectPart;
                } else {
                    // 合并交集结果
                    accumulatedIntersection = turf.union(turf.featureCollection([accumulatedIntersection, intersectPart]));
                }
            }
        });

        // 3. 处理结果
        if (!hasIntersection || !accumulatedIntersection) {
            alert('当前区域与所有参考区域均无重叠，交集为空！');
            return;
        }

        // 4. 将计算后的新形状重新绘制到地图上
        updatePolygonShape(accumulatedIntersection);

    } catch (error) {
        console.error('交集计算失败', error);
        alert('交集计算出错，可能是几何图形过于复杂');
    }
};

// 辅助：根据 GeoJSON 更新当前多边形形状
const updatePolygonShape = (geoJsonPoly: any) => {
    if (!mapInstance || !currentPolygon) return;
    const BMapClass = window.BMapGL || window.BMap;

    let newCoords = [];
    
    if (geoJsonPoly.geometry.type === 'MultiPolygon') {
        alert('计算导致区域断裂，仅保留最大部分');
        newCoords = geoJsonPoly.geometry.coordinates[0][0]; 
    } else {
        newCoords = geoJsonPoly.geometry.coordinates[0];
    }

    // 转换回百度坐标点数组
    const newPath = newCoords.map((c: any) => new BMapClass.Point(c[0], c[1]));
    // 去掉末尾闭合点
    if (newPath.length > 0 && newPath[0].equals(newPath[newPath.length-1])) {
        newPath.pop();
    }

    // 更新覆盖物
    currentPolygon.setPath(newPath);
    
    // 更新中心点 (复用文件2已有的 updateCentroid)
    updateCentroid(); 
    
    alert('区域形状已更新');
};

// ======== 4. 提交修改 ========

const submitModification = async () => {
    if (!currentEditCode.value) {
        alert('未获取到原始数据 Code，无法提交修改');
        return;
    }
    if (!formData.name || !formData.districtCode) {
        alert('请完善名称和行政区编码');
        return;
    }
    if (!hasPolygon.value || !currentPolygon) {
        alert('地图区域为空');
        return;
    }
    if (isSubmitting.value) return;

    try {
        isSubmitting.value = true;

        // 1. 提取几何信息
        const path = currentPolygon.getPath();
        const coords = path.map((p: any) => [p.lng, p.lat]);
        // 闭合
        if (coords.length > 0) {
             const start = coords[0];
             const end = coords[coords.length - 1];
             if (start[0] !== end[0] || start[1] !== end[1]) {
                 coords.push(start);
             }
        }

        // 2. 构造数据
        const geometry = {
            type: "MultiPolygon",
            coordinates: [[coords]]
        };
        
        // 计算 Center
        const center = turf.centerOfMass(turf.polygon([coords]));

        const payload = {
            code: currentEditCode.value, // 必传：作为更新依据
            name: formData.name,
            district_code: formData.districtCode,
            center_point: center.geometry,
            geometry: geometry,
            properties: formData.properties // 原样或修改后传回
        };
        console.log('准备提交的修改数据:', payload);

        const res: any = await updatePrecinct(payload);
        alert('提交修改成功！');
        // console.log('修改接口返回:', res);

        // // 假设后端返回标准格式
        // if (res.code === 200 || res.success) { // 根据实际后端返回调整
        //     alert('提交修改成功！');
        // } else {
        //     alert(res.message || '修改成功'); // 有时候后端直接返回200但没有包装
        // }

    } catch (error) {
        console.error('提交失败', error);
        alert('提交失败，请检查网络或数据格式');
    } finally {
        isSubmitting.value = false;
    }
};


// ======== 生命周期 ========
onMounted(async () => {
    await initMap(MAP_CONTAINER_ID, {
        center: { lng: 121.480540, lat: 31.235929 },
        zoom: 12,
        enableScrollWheelZoom: true
    });

    // 初始化 BMapDraw 工具
    const BMapDraw = await import('bmap-draw');
    drawScene = new BMapDraw.DrawScene(mapInstance.value, { noLimit: true });
    
    polygonDraw = new BMapDraw.PolygonDraw(drawScene, {
        autoViewport: false,
        isSeries: false,
        isOpen: false
    });
    polygonDraw.addEventListener('operate-ok', (e: any) => handleDrawComplete(e.target.overlay));

    polygonEdit = new BMapDraw.PolygonEdit(drawScene);
    polygonEdit.addEventListener('operate-ok', () => {
        isEditing.value = false;
        updateCentroid();
    });
});

onUnmounted(() => {
    destroyMap();
});
</script>

<template>
    <div class="visual-layout">
        <!-- 左侧面板 -->
        <aside class="left-panel">
            <div class="panel-content">
                <h2>编辑板块数据</h2>

                <!-- 新增：搜索区域 -->
                <div class="search-section">
                    <label>输入现有板块 Code</label>
                    <div class="input-group">
                        <input type="text" v-model="searchCode" placeholder="请输入 Code..." @keyup.enter="handleSearch" />
                        <button @click="handleSearch" :disabled="isSearching">
                            {{ isSearching ? '...' : '确定' }}
                        </button>
                    </div>
                </div>

                <hr class="divider" />
                
                <!-- 编辑区域 (只有搜索到数据后才允许操作) -->
                <div v-if="currentEditCode" class="edit-area">
                    <div class="form-item">
                        <label>板块名称</label>
                        <input type="text" v-model="formData.name" />
                    </div>
                    
                    <div class="form-item">
                        <label>行政区编码</label>
                        <input type="text" v-model="formData.districtCode" />
                    </div>

                    <div class="action-group">
                        <p class="status-text" v-if="hasPolygon">
                            <span class="success">已加载地理围栏</span>
                        </p>
                        <button 
                            class="btn-primary" 
                            :class="{ 'btn-danger': hasPolygon }"
                            @click="toggleDrawMode"
                        >
                            {{ hasPolygon ? '清除并重新绘制' : '绘制区域' }}
                        </button>
                    </div>

                    <div class="action-group" v-if="hasPolygon">
                        <button 
                            class="btn-secondary" 
                            :class="{ 'btn-editing': isEditing }"
                            @click="toggleEditMode"
                        >
                            {{ isEditing ? '完成调整' : '调整形状' }}
                        </button>
                    </div>

                    <!-- 多边形差集/交集计算工具 -->
                    <div class="ref-section">
                        <h3>多边形差集计算</h3>
                        
                        <!-- 参考街道 -->
                        <div class="input-group">
                            <input type="text" v-model="refStreetCode" placeholder="输入街道代码" />
                            <button @click="fetchReferenceData('street')">确定</button>
                        </div>

                        <!-- 参考板块 -->
                        <div class="input-group">
                            <input type="text" v-model="refBlockCode" placeholder="输入板块代码" />
                            <button @click="fetchReferenceData('block')">确定</button>
                        </div>

                        <!-- 操作按钮 -->
                        <div class="action-row">
                            <button class="btn-secondary small" @click="clearReferenceLayers">清空参考</button>
                            
                            <button 
                                class="btn-primary small" 
                                :disabled="!hasPolygon"
                                @click="calculateDifference"
                            >
                                取差集
                            </button>

                            <button 
                                class="btn-primary small" 
                                :disabled="!hasPolygon"
                                @click="calculateIntersection"
                            >
                                取交集
                            </button>
                        </div>

                                            <!-- [新增] 反向更新按钮行 -->
                    <div class="action-row">
                        <button 
                            class="btn-danger small" 
                            :disabled="!hasPolygon || isSubmitting"
                            @click="calculateReverseDifferenceAndUpdate"
                            style="width: 100%; background-color: #fa8c16; border-color: #fa8c16; color: white;"
                        >
                            {{ isSubmitting ? '更新中...' : '反向裁剪并更新参考库' }}
                        </button>
                    </div>
                    </div>

                    <div class="footer-action">
                        <button class="btn-submit" @click="submitModification" :disabled="isSubmitting">
                            {{ isSubmitting ? '提交中...' : '提交修改' }}
                        </button>
                    </div>
                </div>
                
                <div v-else class="empty-state">
                    请先在上方输入 Code 获取数据
                </div>

            </div>
        </aside>

        <!-- 右侧地图 -->
        <main class="right-map">
            <div :id="MAP_CONTAINER_ID" class="map-container"></div>
        </main>
    </div>
</template>

<style scoped lang="scss">
/* 复用大部分样式，新增部分如下 */

.visual-layout {
    display: flex;
    width: 100%;
    height: 100vh;
    
    .left-panel {
        width: 340px;
        background: #fff;
        display: flex;
        flex-direction: column;
        box-shadow: 2px 0 10px rgba(0,0,0,0.1);
        z-index: 20;

        .panel-content {
            padding: 20px;
            overflow-y: auto;
        }
    }
    .right-map {
        flex: 1;
        .map-container { width: 100%; height: 100%; }
    }
}

.search-section {
    background: #f9f9f9;
    padding: 15px;
    border-radius: 6px;
    border: 1px solid #eee;
    
    label {
        font-size: 12px;
        color: #666;
        margin-bottom: 8px;
        display: block;
    }
}

.input-group {
    display: flex;
    input {
        flex: 1;
        padding: 8px;
        border: 1px solid #ddd;
        border-radius: 4px 0 0 4px;
        outline: none;
        &:focus { border-color: #1890ff; }
    }
    button {
        padding: 0 15px;
        background: #1890ff;
        color: #fff;
        border: none;
        border-radius: 0 4px 4px 0;
        cursor: pointer;
        &:disabled { background: #ccc; }
        &:hover:not(:disabled) { background: #40a9ff; }
    }
}

.divider {
    margin: 20px 0;
    border: 0;
    border-top: 1px solid #eee;
}

.form-item {
    margin-bottom: 15px;
    label { display: block; margin-bottom: 5px; font-weight: 500; font-size: 14px; }
    input { 
        width: 100%; padding: 8px; box-sizing: border-box; 
        border: 1px solid #ddd; border-radius: 4px;
    }
}

.status-text {
    font-size: 12px;
    margin-bottom: 5px;
    .success { color: #52c41a; }
}

button {
    cursor: pointer;
    transition: all 0.3s;
}

.btn-primary {
    width: 100%;
    padding: 10px;
    background: #1890ff;
    color: #fff;
    border: none;
    border-radius: 4px;
    &.btn-danger { background: #ff4d4f; &:hover { background: #ff7875; } }
}

.btn-secondary {
    width: 100%;
    padding: 10px;
    background: #f0f0f0;
    border: none;
    border-radius: 4px;
    color: #333;
    &.btn-editing { background: #faad14; color: #fff; }
}

.btn-submit {
    width: 100%;
    padding: 12px;
    background: #52c41a;
    color: #fff;
    border: none;
    border-radius: 4px;
    font-weight: bold;
    margin-top: 20px;
    &:hover { background: #73d13d; }
    &:disabled { background: #b7eb8f; cursor: not-allowed; }
}

.empty-state {
    text-align: center;
    color: #999;
    margin-top: 40px;
}

.ref-section {
    margin-top: 20px;
    padding-top: 20px;
    border-top: 1px solid #eee;
    h3 { font-size: 14px; margin-bottom: 10px; }
    .action-row {
        display: flex;
        gap: 10px;
        margin-top: 10px;
        .small { font-size: 12px; padding: 6px; }
    }
}
</style>
