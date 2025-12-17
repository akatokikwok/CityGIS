<script setup lang="ts">
import { onMounted, onUnmounted, reactive, ref } from 'vue';
import { useBaiduMap } from '@/composable/useBaiduMap'; 
import * as turf from '@turf/turf';
import { 
    getSubdistrictByCode, // 替换为街道查询
    updateSubdistrict,    // 替换为街道更新
    getGeoByCode          // 用于获取参考围栏
} from '@/api/geoApi'; 

// 使用封装好的 hook
const { mapInstance, initMap, destroyMap } = useBaiduMap();
const MAP_CONTAINER_ID = 'subdistrict-map-edit-container';

// --- 变量定义 ---
let drawScene: any = null;
let polygonDraw: any = null;
let polygonEdit: any = null;
let currentPolygon: any = null; 
let referenceLayers: any[] = []; 
// [新增] 用于缓存参考区域的原始 GeoJSON 数据
const referenceDataCache = new Map<string, { type: 'subdistrict' | 'precinct', geometry: any }>();
let centroidMarker: any = null; 

const isSubmitting = ref(false);
const isSearching = ref(false);
const isEditing = ref(false); 
const isDrawing = ref(false); 
const hasPolygon = ref(false); 

// 搜索相关
const searchCode = ref('');
const currentEditCode = ref(''); 

// 参考图层输入框
const refOtherCode = ref(''); // 输入其他街道或板块的 code

// 表单数据
const formData = reactive({
    name: '',
    districtCode: ''
});

// ======== 1. 核心：加载与回显逻辑 ========

const handleSearch = async () => {
    if (!searchCode.value) {
        alert('请输入街道 Code');
        return;
    }
    
    if (hasPolygon.value) {
        if (!confirm('当前地图上已有正在编辑的数据，重新搜索将覆盖，是否继续？')) {
            return;
        }
    }

    try {
        isSearching.value = true;
        clearAll();

        // [修改点] 调用街道详情接口
        const res: any = await getSubdistrictByCode(searchCode.value);
        console.log('接口返回数据:', res);
        // 注意：根据你的 axios 封装，这里可能直接返回 data，也可能在 data.data 里，请根据实际情况解构
        const data = res.data || res; 
        
        console.log('获取街道数据:', data);

        if (!data || !data.code) {
            alert('未找到该街道数据');
            return;
        }

        // 1. 回填表单数据 (只读展示用)
        formData.name = data.name;
        formData.districtCode = data.district_code;
        currentEditCode.value = data.code; 

        // 2. 回显多边形
        if (data.geometry) {
            renderPolygonFromGeoJSON(data.geometry);
        }
        
        // 3. 自动定位
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

// 渲染 GeoJSON (逻辑保持一致)
const renderPolygonFromGeoJSON = (geometry: any) => {
    const BMapClass = window.BMapGL || window.BMap;
    let coords = [];

    if (geometry.type === 'MultiPolygon') {
        if (geometry.coordinates && geometry.coordinates.length > 0) {
            coords = geometry.coordinates[0][0];
        }
    } else if (geometry.type === 'Polygon') {
        coords = geometry.coordinates[0];
    }

    if (coords.length === 0) return;

    const path = coords.map((c: any) => new BMapClass.Point(c[0], c[1]));
    
    // 去重末尾点
    const start = path[0];
    const end = path[path.length - 1];
    if (start.equals(end)) {
        path.pop();
    }

    const polygon = new BMapClass.Polygon(path, {
        strokeColor: '#fa541c', // 换个颜色区分街道 (橙色)
        fillColor: '#fa541c',
        strokeWeight: 2,
        strokeOpacity: 0.8,
        fillOpacity: 0.3
    });

    mapInstance.value.addOverlay(polygon);
    currentPolygon = polygon;
    hasPolygon.value = true;
    
    polygon.addEventListener('lineupdate', updateCentroid);
    updateCentroid();
};

const clearAll = () => {
    clearCurrentPolygon();
    currentEditCode.value = '';
    formData.name = '';
    formData.districtCode = '';
};

// ======== 2. 交互逻辑 (绘制/编辑/删除) - 保持不变 ========

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
    
    // 绘制完成后设置样式
    overlay.setStrokeColor('#fa541c');
    overlay.setFillColor('#fa541c');
    
    updateCentroid();
    overlay.addEventListener('lineupdate', updateCentroid);
};

const clearCurrentPolygon = () => {
    if (isEditing.value && polygonEdit) {
        polygonEdit.close();
        isEditing.value = false;
    }
    if (drawScene && currentPolygon) {
        mapInstance.value.removeOverlay(currentPolygon); 
        currentPolygon = null;
    }
    if (centroidMarker) {
        mapInstance.value.removeOverlay(centroidMarker);
        centroidMarker = null;
    }
    hasPolygon.value = false;
};

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

const updateCentroid = () => {
    if (!currentPolygon) return;
    const path = currentPolygon.getPath();
    if (!path || path.length < 3) return;

    const coords = path.map((p: any) => [p.lng, p.lat]);
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
            const marker = new BMapClass.Marker(point, { title: '街道中心点' });
            mapInstance.value.addOverlay(marker);
            centroidMarker = marker;
        }
    } catch (e) {
        console.error(e);
    }
};

// ======== 3. 差集/交集计算逻辑 (通用逻辑) ========

// Fetch 参考数据
// type 用于后端区分或前端调用不同接口，这里为了通用演示，统一处理
const fetchReferenceData = async (type: 'subdistrict' | 'precinct') => {
    const code = refOtherCode.value;
    if (!code) {
        alert('请输入参考区域编码');
        return;
    }

    try {
        // [修改点] 使用 api/geoApi 中定义的接口
        const res: any = await getGeoByCode(code, type);
        const data = res.data || res;

        if (!data || !data.geometry) {
            alert('未找到参考区域几何数据');
            return;
        }
        
        // [新增] 存入缓存，供反向更新使用
        referenceDataCache.set(code, { type, geometry: data.geometry });

        drawReferencePolygon(data.geometry);
        refOtherCode.value = '';
       
    } catch (error) {
        console.error('请求参考数据出错', error);
        alert('获取参考数据失败');
    }
};

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
        const outerRing = polyCoords[0]; 
        const points = outerRing.map((coord: any) => new BMapClass.Point(coord[0], coord[1]));
        
        const polygonOverlay = new BMapClass.Polygon(points, {
            strokeColor: '#52c41a', // 绿色参考线
            fillColor: '#52c41a',
            strokeWeight: 2,
            strokeOpacity: 0.8,
            fillOpacity: 0.15,
            strokeStyle: 'dashed'
        });
        
        mapInstance.value.addOverlay(polygonOverlay);
        referenceLayers.push(polygonOverlay); 
    });
};

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

            // 5. 调用更新 API (根据 type 决定 URL)
            // 注意：这里假设你的后端服务运行在 localhost:3000，如果使用了代理，请改为相对路径或配置好的 baseUrl
            const apiUrl = type === 'subdistrict' 
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

// --- 差集计算 (逻辑保持不变) ---
const calculateDifference = () => {
    if (!hasPolygon.value || !currentPolygon) {
        alert('请先绘制/加载街道区域');
        return;
    }
    if (referenceLayers.length === 0) {
        alert('当前没有参考区域，无法取差集');
        return;
    }

    try {
        const currentPath = currentPolygon.getPath();
        const currentCoords = currentPath.map((p: any) => [p.lng, p.lat]);
        if (currentCoords.length > 0 && (currentCoords[0][0] !== currentCoords[currentCoords.length-1][0])) {
            currentCoords.push(currentCoords[0]);
        }
        let mainPoly = turf.polygon([currentCoords]);

        referenceLayers.forEach(layer => {
            const path = layer.getPath();
            const coords = path.map((p: any) => [p.lng, p.lat]);
            if (coords.length > 0 && (coords[0][0] !== coords[coords.length-1][0])) {
                coords.push(coords[0]);
            }
            const refPoly = turf.polygon([coords]);
            const diff = turf.difference(turf.featureCollection([mainPoly, refPoly])); 
            
            if (diff) {
                mainPoly = diff;
            } else {
                alert('警告：区域被完全减去！');
                mainPoly = null;
                return;
            }
        });

        if (!mainPoly) return;
        updatePolygonShape(mainPoly);
    } catch (error) {
        console.error('差集计算失败', error);
        alert('差集计算出错');
    }
};

// --- 交集计算 (逻辑保持不变) ---
const calculateIntersection = () => {
    if (!hasPolygon.value || !currentPolygon) {
        alert('请先绘制/加载街道区域');
        return;
    }
    if (referenceLayers.length === 0) {
        alert('当前没有参考区域，无法取交集');
        return;
    }

    try {
        const currentPath = currentPolygon.getPath();
        const currentCoords = currentPath.map((p: any) => [p.lng, p.lat]);
        if (currentCoords.length > 0 && (currentCoords[0][0] !== currentCoords[currentCoords.length-1][0])) {
            currentCoords.push(currentCoords[0]);
        }
        const mainPoly = turf.polygon([currentCoords]);

        let accumulatedIntersection: any = null;
        let hasIntersection = false;

        referenceLayers.forEach(layer => {
            const path = layer.getPath();
            const coords = path.map((p: any) => [p.lng, p.lat]);
            if (coords.length > 0 && (coords[0][0] !== coords[coords.length-1][0])) {
                coords.push(coords[0]);
            }
            const refPoly = turf.polygon([coords]);
            const intersectPart = turf.intersect(turf.featureCollection([mainPoly, refPoly]));

            if (intersectPart) {
                hasIntersection = true;
                if (!accumulatedIntersection) {
                    accumulatedIntersection = intersectPart;
                } else {
                    accumulatedIntersection = turf.union(turf.featureCollection([accumulatedIntersection, intersectPart]));
                }
            }
        });

        if (!hasIntersection || !accumulatedIntersection) {
            alert('交集为空！');
            return;
        }
        updatePolygonShape(accumulatedIntersection);
    } catch (error) {
        console.error('交集计算失败', error);
        alert('交集计算出错');
    }
};

const updatePolygonShape = (geoJsonPoly: any) => {
    if (!mapInstance || !currentPolygon) return;
    const BMapClass = window.BMapGL || window.BMap;

    let newCoords = [];
    // 简化处理：只取最大的多边形
    if (geoJsonPoly.geometry.type === 'MultiPolygon') {
        newCoords = geoJsonPoly.geometry.coordinates[0][0]; 
    } else {
        newCoords = geoJsonPoly.geometry.coordinates[0];
    }

    const newPath = newCoords.map((c: any) => new BMapClass.Point(c[0], c[1]));
    if (newPath.length > 0 && newPath[0].equals(newPath[newPath.length-1])) {
        newPath.pop();
    }

    currentPolygon.setPath(newPath);
    updateCentroid(); 
    alert('区域形状已更新');
};

// ======== 4. 提交修改 (按需求定制) ========

const submitModification = async () => {
    if (!currentEditCode.value) {
        alert('未获取到原始数据 Code，无法提交');
        return;
    }
    if (!hasPolygon.value || !currentPolygon) {
        alert('地图区域为空');
        return;
    }
    if (isSubmitting.value) return;

    try {
        isSubmitting.value = true;

        // 1. 提取几何
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

        const geometry = {
            type: "MultiPolygon",
            coordinates: [[coords]]
        };
        
        // 2. 计算中心点
        const center = turf.centerOfMass(turf.polygon([coords]));

        // [修改点] 仅提交 code, geometry, center_point
        const payload = {
            code: currentEditCode.value, 
            center_point: center.geometry,
            geometry: geometry
        };
        
        console.log('提交街道更新:', payload);

        // [修改点] 调用街道更新接口
        await updateSubdistrict(payload);
        
        alert('街道围栏更新成功！');

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
        center: { lng: 121.48, lat: 31.23 }, // 默认上海
        zoom: 12,
        enableScrollWheelZoom: true
    });

    // 动态加载绘制工具
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
        <aside class="left-panel">
            <div class="panel-content">
                <!-- [修改点] 标题 -->
                <h2>编辑街道(Subdistrict)围栏</h2>

                <div class="search-section">
                    <label>输入街道 Code</label>
                    <div class="input-group">
                        <input type="text" v-model="searchCode" placeholder="请输入 Code..." @keyup.enter="handleSearch" />
                        <button @click="handleSearch" :disabled="isSearching">
                            {{ isSearching ? '...' : '确定' }}
                        </button>
                    </div>
                </div>

                <hr class="divider" />
                
                <div v-if="currentEditCode" class="edit-area">
                    <!-- [修改点] 字段设为只读 -->
                    <div class="form-item">
                        <label>街道名称 (不可编辑)</label>
                        <input type="text" v-model="formData.name" disabled readonly class="input-readonly" />
                    </div>
                    
                    <div class="form-item">
                        <label>行政区编码 (不可编辑)</label>
                        <input type="text" v-model="formData.districtCode" disabled readonly class="input-readonly" />
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

                    <!-- 差集/交集计算工具 -->
                    <div class="ref-section">
                        <h3>高级计算 (差集/交集)</h3>
                        <p class="hint">可输入其他街道或板块代码作为参考</p>
                        
                        <div class="input-group">
                            <input type="text" v-model="refOtherCode" placeholder="输入参考 Code" />
                            <!-- 提供两个按钮，分别作为街道或板块加载，增加灵活性 -->
                            <button @click="fetchReferenceData('subdistrict')" title="作为街道加载">街道</button>
                            <button @click="fetchReferenceData('precinct')" title="作为板块加载">板块</button>
                        </div>

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
                            {{ isSubmitting ? '提交中...' : '保存街道围栏' }}
                        </button>
                    </div>
                </div>
                
                <div v-else class="empty-state">
                    请先在上方输入街道 Code 获取数据
                </div>

            </div>
        </aside>

        <main class="right-map">
            <div :id="MAP_CONTAINER_ID" class="map-container"></div>
        </main>
    </div>
</template>

<style scoped lang="scss">
/* 基本复用之前的样式，增加只读输入框样式 */
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
    label { font-size: 12px; color: #666; margin-bottom: 8px; display: block; }
}

.input-group {
    display: flex;
    input {
        flex: 1; padding: 8px; border: 1px solid #ddd; border-radius: 4px 0 0 4px; outline: none;
        &:focus { border-color: #fa541c; } // 街道用橙色系
    }
    button {
        padding: 0 10px; background: #fa541c; color: #fff; border: none; cursor: pointer;
        &:first-of-type { border-right: 1px solid rgba(255,255,255,0.3); }
        &:last-child { border-radius: 0 4px 4px 0; }
        &:disabled { background: #ccc; }
    }
}

.divider { margin: 20px 0; border: 0; border-top: 1px solid #eee; }

.form-item {
    margin-bottom: 15px;
    label { display: block; margin-bottom: 5px; font-weight: 500; font-size: 14px; }
    input { 
        width: 100%; padding: 8px; box-sizing: border-box; 
        border: 1px solid #ddd; border-radius: 4px;
        // 只读样式
        &.input-readonly { background: #f5f5f5; color: #888; cursor: not-allowed; }
    }
}

.status-text { font-size: 12px; margin-bottom: 5px; .success { color: #52c41a; } }

button { cursor: pointer; transition: all 0.3s; }

.btn-primary {
    width: 100%; padding: 10px; background: #fa541c; color: #fff; border: none; border-radius: 4px;
    &.btn-danger { background: #ff4d4f; }
}

.btn-secondary {
    width: 100%; padding: 10px; background: #f0f0f0; border: none; border-radius: 4px; color: #333;
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
    font-size: 14px;
}

.ref-section {
    margin-top: 20px;
    padding-top: 20px;
    border-top: 1px solid #eee;
    
    h3 { font-size: 14px; margin-bottom: 5px; color: #333; }
    
    .hint {
        font-size: 12px;
        color: #999;
        margin-bottom: 10px;
    }

    .action-row {
        display: flex;
        gap: 8px;
        margin-top: 10px;
        
        .small { 
            font-size: 12px; 
            padding: 6px 10px; 
            flex: 1; // 让按钮平分宽度
        }
    }
}
</style>    