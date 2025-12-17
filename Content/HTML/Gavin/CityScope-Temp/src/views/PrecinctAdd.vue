<script setup lang="ts">
import { onMounted, onUnmounted, reactive, ref } from 'vue';
import { useBaiduMap } from '@/composable/useBaiduMap';
import * as turf from '@turf/turf'; 
import { addPrecinct } from '@/api/geoApi'; // 确保路径对应你的实际项目结构

// 使用封装好的 hook
const { mapInstance, initMap, destroyMap, isMapReady } = useBaiduMap();
const MAP_CONTAINER_ID = 'visual-map-container';

// --- 1. 数据定义 ---
let drawScene: any = null;
let polygonDraw: any = null;
let polygonEdit: any = null;
let currentPolygon: any = null;
let referenceLayers: any[] = [];

// [新增] 用于缓存参考区域的原始 GeoJSON 数据，key为code，value为{ type, geometry }
const referenceDataCache = new Map<string, { type: 'street' | 'block', geometry: any }>();

// 建议添加一个 loading 状态防止重复点击
const isSubmitting = ref(false);

const refStreetCode = ref(''); // 街道参考输入框
const refBlockCode = ref('');  // 板块参考输入框


let centroidMarker: any = null; // 存放形心点 Marker
const isEditing = ref(false); // 是否处于编辑模式

// 表单数据
const formData = reactive({
    name: '',
    districtCode: ''
}); 

// 界面状态
const isDrawing = ref(false); // 是否正在绘制模式
const hasPolygon = ref(false); // 是否已经存在绘制好的区域

// ======== 核心交互逻辑 ========

// 点击“添加区域” / “取消区域”
const toggleDrawMode = () => {
    if (hasPolygon.value) {
        // 逻辑：取消区域 (清除已画的)
        clearCurrentPolygon();
    } else {
        // 逻辑：添加区域 (开启绘制)
        startDrawing();
    }
};

// 开始绘制
const startDrawing = () => {
    if (polygonDraw === null) return;
    if (polygonDraw.isOpen) return; // 已经开启绘制则不重复开启
    polygonDraw.open();
};

// 绘制完成处理
const handleDrawComplete = (overlay: any) => {
    // 关闭绘制模式
    polygonDraw.close();
    isDrawing.value = false;
    
    // 保存当前多边形
    currentPolygon = overlay;
    hasPolygon.value = true;

    // 下一步我们会在这里计算形心
    updateCentroid();

    overlay.addEventListener('lineupdate', updateCentroid);
};

// 清除当前区域
const clearCurrentPolygon = () => {
    if (drawScene  && currentPolygon) {
        if (isEditing.value) {
            currentPolygon.disableEditing();
            isEditing.value = false;
        }

        drawScene.removeOverlay(currentPolygon);
        currentPolygon = null;
    }

    if (centroidMarker) {
        mapInstance.value.removeOverlay(centroidMarker);
        centroidMarker = null;
    }
    
    hasPolygon.value = false;
    isDrawing.value = false;

    if (polygonDraw && polygonDraw.isOpen) {
        polygonDraw.close();
    }
    if (polygonEdit && isEditing.value) {
        polygonEdit.close();
        isEditing.value = false;
    }
};

// --- [核心新增] 计算形心并更新 Marker 位置 ---
const updateCentroid = () => {
    if (!currentPolygon) return;

    // 1. 获取百度地图多边形的路径点
    const path = currentPolygon.getPath(); // 返回 Array<Point>
    if (!path || path.length < 3) return; // 至少3个点才能构成多边形

    // 2. 转换为 GeoJSON 格式的坐标数组 [lng, lat]
    // Turf 要求多边形首尾闭合，即第一个点和最后一个点相同
    const coords = path.map((p: any) => [p.lng, p.lat]);
    const startPoint = coords[0];
    const endPoint = coords[coords.length - 1];
    
    // 检查是否闭合，如果不闭合则手动添加第一个点到末尾
    if (startPoint[0] !== endPoint[0] || startPoint[1] !== endPoint[1]) {
        coords.push(startPoint);
    }

    try {
        // 3. 使用 Turf 计算中心点
        const polygonGeo = turf.polygon([coords]);
        const center = turf.centerOfMass(polygonGeo);
        const [lng, lat] = center.geometry.coordinates;

        // 4. 在地图上绘制或更新 Marker
        // 这里假设全局变量是 BMapGL (如果使用的是老版 BMap，请改为 BMap)
        // bmap-draw 通常配合 BMapGL 或 BMap 使用，根据你的环境调整
        const BMapClass = window.BMapGL;
        const point = new BMapClass.Point(lng, lat);

        if (centroidMarker) {
            // 如果 Marker 已存在，只更新位置
            centroidMarker.setPosition(point);
        } else {
            // 如果 Marker 不存在，创建新的
            const marker = new BMapClass.Marker(point, {
                title: '板块中心点'
            });
            mapInstance.value.addOverlay(marker);
            centroidMarker = marker;
        }
        
        // 保存计算出的中心点坐标，供最后提交使用
        // formData.centerPoint = center.geometry; (先存内存里，最后一步再整合)
        
    } catch (error) {
        console.error('形心计算失败', error);
    }
};

// --- [新增] 切换编辑模式 ---
const toggleEditMode = () => {
    if (!currentPolygon) return;

    if (isEditing.value) {
        // 完成调整：关闭编辑
        if (!polygonEdit) return;
        polygonEdit.close();
        isEditing.value = false;
        // 确保最后位置是最新的
        updateCentroid(); 
    } else {
        // 开始调整：开启编辑
        if (!polygonEdit) return;
        polygonEdit.open(currentPolygon);
        isEditing.value = true;
    }
};


// 调整区域 (占位，下一步实现)
const editPolygon = () => {
    if (!polygonEdit || isEditing.value) return;
    isEditing.value = true;
    polygonEdit.open(currentPolygon);
};

// --- 5. 提交数据 ---
const submitData = async () => {
    // 1. 基础校验
    if (!formData.name) {
        alert('请填写板块名称');
        return;
    }
    if (!formData.districtCode) {
        alert('请填写行政区编码');
        return;
    }
    if (!hasPolygon.value || !currentPolygon) {
        alert('请先在地图上绘制板块区域');
        return;
    }
     if (isSubmitting.value) return;


    try {
        isSubmitting.value = true; // 开启 loading
        // 2. 提取几何数据 (Geometry)
        // 获取路径点
        const path = currentPolygon.getPath();
        // 转换为 [[lng, lat], [lng, lat]...]
        const coords = path.map((p: any) => [p.lng, p.lat]);
        
        // 闭合多边形 (首尾点一致)
        if (coords.length > 0) {
            const start = coords[0];
            const end = coords[coords.length - 1];
            if (start[0] !== end[0] || start[1] !== end[1]) {
                coords.push(start);
            }
        }

        // 构造符合 GeoJSON 标准的 MultiPolygon 结构
        // 我们的编辑器只支持画一个闭合环，所以结构是 [[[[x,y]...]]]
        // 如果有内环(挖孔)会更复杂，但这里简化为单一外环
        const geometry = {
            type: "MultiPolygon",
            coordinates: [[coords]] 
        };

        // 3. 提取中心点数据 (Center Point)
        // 我们直接复用 Turf 计算出的中心点，或者重新计算一次以保万全
        const polygonGeo = turf.polygon([coords]);
        const center = turf.centerOfMass(polygonGeo);
        const centerPoint = center.geometry; // { type: "Point", coordinates: [lng, lat] }

        // 4. 组装最终 Payload
        const payload = {
            name: formData.name,
            district_code: formData.districtCode,
            center_point: centerPoint,
            geometry: geometry
        };

        console.log('准备提交的数据:', JSON.stringify(payload, null, 2));
        await addPrecinct(payload);
        alert('提交成功！板块已添加。');
    } catch (error) {
        console.error('数据组装失败', error);
        alert('数据处理出错，请检查控制台');
    } finally {
        isSubmitting.value = false;
    }


};


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

    const BMapClass = window.BMapGL;
    
    // 解析 GeoJSON 坐标
    // 注意：geometry.coordinates 可能是 [[[x,y]...]] (Polygon) 或 [[[[x,y]...]]] (MultiPolygon)
    // 为简化，这里处理 MultiPolygon 的第一层，或者遍历所有多边形
    
    let polygonsToDraw = [];
    
    if (geometry.type === 'MultiPolygon') {
        polygonsToDraw = geometry.coordinates;
    } else if (geometry.type === 'Polygon') {
        polygonsToDraw = [geometry.coordinates];
    }

    polygonsToDraw.forEach((polyCoords: any) => {
        // polyCoords[0] 通常是外圈，polyCoords[1...] 是内圈(镂空)
        // 百度地图JSAPI处理镂空比较复杂，这里根据需求"只展示外围"
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
        
        // 禁止参考区域被点击或编辑，以免干扰
        // polygonOverlay.disableMassClear(); // 可选：防止被 map.clearOverlays() 一波带走，如果需要的话
        
        mapInstance.value.addOverlay(polygonOverlay);
        referenceLayers.push(polygonOverlay); // 存入数组以便后续清除
    });
};

// --- 3. 清空参考区域 ---
// [修改] 清空参考区域时，同时也清空缓存
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
        alert('请先绘制本地区域');
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

        // 1. 准备“刀具”：当前编辑的区域 (Current Polygon)
        const currentPath = currentPolygon.getPath();
        const currentCoords = currentPath.map((p: any) => [p.lng, p.lat]);
        if (currentCoords[0][0] !== currentCoords[currentCoords.length - 1][0]) {
            currentCoords.push(currentCoords[0]);
        }
        const cutterPoly = turf.polygon([currentCoords]);

        // 2. 遍历缓存的参考数据
        for (const [code, refData] of referenceDataCache) {
            const { type, geometry: originalGeo } = refData;
            
            // 将原始 GeoJSON 转为 Turf 对象
            // 注意：originalGeo 可能是 Polygon 或 MultiPolygon
            let refEntity: any;
            if (originalGeo.type === 'Polygon') {
                refEntity = turf.polygon(originalGeo.coordinates);
            } else if (originalGeo.type === 'MultiPolygon') {
                refEntity = turf.multiPolygon(originalGeo.coordinates);
            } else {
                continue; // 不支持的类型跳过
            }

            // 3. 执行差集运算：参考区域 - 当前区域
            const diffResult = turf.difference(turf.featureCollection([refEntity, cutterPoly]));

            // 如果没有重叠（结果没变），或者完全被切没了（null），视情况处理
            // 这里假设如果不重叠就不提交更新，如果切没了则可能有问题，需谨慎处理
            if (!diffResult) {
                console.warn(`参考区域 ${code} 被完全移除，跳过更新或需人工介入`);
                continue;
            }

            // 简单判断面积是否发生变化，如果没变则不调用接口节省资源 (可选)
            // const originalArea = turf.area(refEntity);
            // const newArea = turf.area(diffResult);
            // if (Math.abs(originalArea - newArea) < 1) continue; 

            // 4. 格式化数据准备提交
            // 确保 geometry 是 MultiPolygon 格式
            let finalGeometry = diffResult.geometry;
            if (finalGeometry.type === 'Polygon') {
                finalGeometry = {
                    type: 'MultiPolygon',
                    coordinates: [finalGeometry.coordinates]
                };
            }

            // 计算新的中心点 center_point
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
        // 可选：自动清空重载，或者只清空
        clearReferenceLayers();

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
        if (currentCoords[0][0] !== currentCoords[currentCoords.length-1][0]) {
            currentCoords.push(currentCoords[0]);
        }
        let mainPoly = turf.polygon([currentCoords]);

        // 2. 遍历所有参考区域，依次进行差集运算 (Main - Ref1 - Ref2 ...)
        // 注意：我们需要把百度地图的覆盖物转回 GeoJSON 坐标
        referenceLayers.forEach(layer => {
            const path = layer.getPath();
            const coords = path.map((p: any) => [p.lng, p.lat]);
            if (coords[0][0] !== coords[coords.length-1][0]) {
                coords.push(coords[0]);
            }
            const refPoly = turf.polygon([coords]);
            console.log(refPoly);
            console.log(mainPoly);
            // 执行差集运算
            const diff = turf.difference(turf.featureCollection([mainPoly, refPoly])); 
            
            if (diff) {
                mainPoly = diff; // 更新主区域为差集后的结果
            } else {
                // 如果结果为空，说明主区域完全在参考区域内部，被减没了
                alert('警告：区域被完全减去！');
                mainPoly = null; // 标记为空
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
        if (currentCoords[0][0] !== currentCoords[currentCoords.length-1][0]) {
            currentCoords.push(currentCoords[0]);
        }
        const mainPoly = turf.polygon([currentCoords]);

        // 用于存储所有产生的交集部分
        let accumulatedIntersection: any = null;
        let hasIntersection = false;

        // 2. 遍历所有参考区域
        // 逻辑：找出 Main 与 每个 Ref 的交集，然后把这些交集拼在一起 (Union)
        referenceLayers.forEach(layer => {
            const path = layer.getPath();
            const coords = path.map((p: any) => [p.lng, p.lat]);
            if (coords[0][0] !== coords[coords.length-1][0]) {
                coords.push(coords[0]);
            }
            const refPoly = turf.polygon([coords]);

            // --- 核心计算：取当前参考区域与主区域的交集 ---
            // 注意：Turf v6/v7 的 intersect 参数通常接受 FeatureCollection 或两个 Feature
            // 这里沿用你代码中的 featureCollection 写法
            const intersectPart = turf.intersect(turf.featureCollection([mainPoly, refPoly]));

            if (intersectPart) {
                hasIntersection = true;
                if (!accumulatedIntersection) {
                    // 第一个有效的交集，直接作为起始结果
                    accumulatedIntersection = intersectPart;
                } else {
                    // 后续的交集，需要合并到总结果中 (取并集)
                    // 相当于：Result = Result + (Main ∩ Ref_Current)
                    accumulatedIntersection = turf.union(turf.featureCollection([accumulatedIntersection, intersectPart]));
                }
            }
        });

        // 3. 处理结果
        if (!hasIntersection || !accumulatedIntersection) {
            alert('当前区域与所有参考区域均无重叠，交集为空！');
            // 可以选择清空当前图形，或者不做操作
            // updatePolygonShape(null); 
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
    const BMapClass = window.BMapGL;

    // 处理结果可能是 MultiPolygon (如果被切成了两半) 或 Polygon
    // 为了简化，如果变成了多个，我们只取面积最大的那一块，或者提示用户
    let newCoords = [];
    
    if (geoJsonPoly.geometry.type === 'MultiPolygon') {
        alert('差集计算导致区域断裂，仅保留最大部分');
        // 简单的取第一个部分，或者你可以遍历找面积最大的
        newCoords = geoJsonPoly.geometry.coordinates[0][0]; 
    } else {
        newCoords = geoJsonPoly.geometry.coordinates[0];
    }

    // 转换回百度坐标点数组
    const newPath = newCoords.map((c: any) => new BMapClass.Point(c[0], c[1]));
    // 去掉末尾闭合点（百度地图不需要重复最后一个点）
    newPath.pop(); 

    // 更新覆盖物
    currentPolygon.setPath(newPath);
    
    // 别忘了更新中心点
    updateCentroid(); 
    
    alert('差集计算完成，区域已更新');
};


// 页面挂载时初始化地图
onMounted(async () => {
    await initMap(MAP_CONTAINER_ID, {
        center: { lng: 121.480540, lat: 31.235929 },
        zoom: 10,
        enableScrollWheelZoom: true
    }).catch(err => {
        console.error('地图初始化失败', err);
    });

    // 动态引入 bmap-draw
    const BMapDraw = await import('bmap-draw');
    // 初始化绘制管理器
    drawScene = new BMapDraw.DrawScene(mapInstance.value, {
        noLimit: true // 不限制绘制范围
    });
    polygonDraw = new BMapDraw.PolygonDraw(drawScene, {
        autoViewport: true, // 是否自动调整视野
        hideTip: true, // 隐藏绘制提示
        isSeries: true, // 是否开启批量绘制
        isOpen: false
    });

    polygonDraw.addEventListener('operate-ok', (e) => {
        console.log('operate-ok', e.target);
        handleDrawComplete(e.target.overlay);
    });

    polygonEdit = new BMapDraw.PolygonEdit(drawScene);
    polygonEdit.addEventListener('operate-ok', (e) => {
        console.log('operate-ok', e.target);
        isEditing.value = false;
        console.log(currentPolygon === e.target.overlay);
        updateCentroid();
    });
    polygonEdit.addEventListener('operate-cancel', (e) => {
        console.log('operate-cancel', e.target);
        isEditing.value = false;
    });
});

// 页面卸载时销毁地图实例，防止内存泄漏
onUnmounted(() => {
    destroyMap();
});
</script>

<template>
    <div class="visual-layout">
        <!-- 左侧面板 -->
        <aside class="left-panel">
            <div class="panel-content">
                <h2>添加板块</h2>
                
                <!-- 1. 基础信息表单 -->
                <div class="form-item">
                    <label>板块名称</label>
                    <input type="text" v-model="formData.name" placeholder="请输入板块名称" />
                </div>
                
                <div class="form-item">
                    <label>所属行政区编码</label>
                    <input type="text" v-model="formData.districtCode" placeholder="例如: 310101" />
                </div>

                <hr class="divider" />

                <!-- 2. 区域绘制控制 -->
                <div class="action-group">
                    <button 
                        class="btn-primary" 
                        :class="{ 'btn-danger': hasPolygon }"
                        @click="toggleDrawMode"
                    >
                        {{ hasPolygon ? '取消区域' : '添加区域' }}
                    </button>
                    <p class="hint" v-if="isDrawing">请在地图上点击绘制多边形，双击结束。</p>
                </div>

                <!-- 3. 区域调整 (仅当有区域时显示) -->
                <div class="action-group" v-if="hasPolygon">
                    <button 
                        class="btn-secondary" 
                        :class="{ 'btn-editing': isEditing }"
                        @click="toggleEditMode"
                    >
                        {{ isEditing ? '完成调整' : '调整区域' }}
                    </button>
                    <p class="hint" v-if="isEditing">拖动多边形的白色节点修改形状，中心点会自动更新。</p>
                </div>

                <!-- 后续的差集计算和提交按钮先占位，保持页面结构整洁 -->
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
                    <button class="btn-submit" @click="submitData">添加板块</button>
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
.visual-layout {
    display: flex;
    width: 100%;
    height: 100vh;
    overflow: hidden;

    .left-panel {
        width: 320px;
        flex-shrink: 0;
        background-color: #fff;
        border-right: 1px solid #e0e0e0;
        box-shadow: 2px 0 8px rgba(0, 0, 0, 0.05);
        z-index: 10;
        display: flex;
        flex-direction: column;
        
        .panel-content {
            padding: 20px;
            flex: 1;
            overflow-y: auto; /* 如果内容过多允许滚动 */
            
            h2 {
                font-size: 20px;
                margin-bottom: 20px;
                color: #333;
                font-weight: 600;
            }

            .form-item {
                margin-bottom: 15px;
                
                label {
                    display: block;
                    font-size: 14px;
                    color: #666;
                    margin-bottom: 5px;
                }
                
                input {
                    width: 100%;
                    padding: 8px 12px;
                    border: 1px solid #ddd;
                    border-radius: 4px;
                    font-size: 14px;
                    outline: none;
                    box-sizing: border-box; // 重要：防止padding撑破宽度
                    
                    &:focus {
                        border-color: #1890ff;
                    }
                }
            }

            .divider {
                border: none;
                border-top: 1px solid #eee;
                margin: 20px 0;
            }

            .action-group {
                margin-bottom: 15px;
                
                .hint {
                    font-size: 12px;
                    color: #ff9800;
                    margin-top: 5px;
                }
            }

            /* 按钮通用样式 */
            button {
                width: 100%;
                padding: 10px;
                border: none;
                border-radius: 4px;
                cursor: pointer;
                font-size: 14px;
                transition: all 0.3s;
                
                &.btn-primary {
                    background-color: #1890ff;
                    color: white;
                    &:hover { background-color: #40a9ff; }
                }

                &.btn-danger {
                    background-color: #ff4d4f;
                    color: white;
                    &:hover { background-color: #ff7875; }
                }

                &.btn-secondary {
                    background-color: #f0f0f0;
                    color: #333;
                    &:hover { background-color: #d9d9d9; }
                }

                &.btn-submit {
                    background-color: #52c41a;
                    color: white;
                    font-weight: bold;
                    margin-top: 20px;
                    &:hover { background-color: #73d13d; }
                }
            }

            .ref-section {
                margin-top: 25px;
                padding-top: 20px;
                border-top: 1px solid #eee;

                h3 {
                    font-size: 16px;
                    margin-bottom: 10px;
                    color: #333;
                }

                .input-group {
                    display: flex;
                    margin-bottom: 10px;
                    
                    input {
                        flex: 1;
                        padding: 6px 10px;
                        border: 1px solid #ddd;
                        border-radius: 4px 0 0 4px;
                        outline: none;
                    }
                    
                    button {
                        width: auto; /* 覆盖之前的 width: 100% */
                        padding: 6px 15px;
                        background-color: #1890ff;
                        color: white;
                        border-radius: 0 4px 4px 0;
                        
                        &:hover {
                            background-color: #40a9ff;
                        }
                    }
                }

                .action-row {
                    display: flex;
                    gap: 10px;
                    margin-top: 10px;

                    button.small {
                        flex: 1;
                        font-size: 12px;
                        padding: 8px 0;
                    }
                }
            }
        }
    }

    .right-map {
        flex: 1;
        position: relative;
        background-color: #f0f2f5;

        .map-container {
            width: 100%;
            height: 100%;
            :deep(.anchorBL) { display: none; }
        }
    }
}
</style>