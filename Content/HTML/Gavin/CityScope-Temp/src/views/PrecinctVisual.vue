<script setup lang="ts">
import { onMounted, onUnmounted, ref } from 'vue';
import { useBaiduMap } from '@/composable/useBaiduMap';
import { View as MapVglView, PolygonLayer, PointLayer, TextLayer } from 'mapvgl';
import axios from 'axios'; // 假设使用 axios，你也可以换成 fetch 或你项目封装的 request

// ======== 类型定义 ========

interface District {
    code: string;
    name: string;
}

// 板块数据结构 (和 subdistrict 类似)
interface Precinct {
    code: string;
    name: string;
    district_code: string;
}

// 地理数据返回结构
interface PrecinctGeoItem {
    name: string;
    center_point: {
        coordinates: [number, number];
    };
    geometry: {
        type: string;
        coordinates: any[];
    };
}

interface DistrictNode extends District {
    children?: Precinct[];
}

// ======== API 定义 (模拟) ========
// 在实际项目中，请将这些放入 api/geoApi.ts
const API_BASE_URL = 'http://localhost:3000';

const getDistrictList = async (): Promise<District[]> => {
    // 假设这是一个现有的 API，如果不是请替换
    // 这里复用你原有的获取行政区逻辑，或者调用你的后端
    const res = await axios.get(`${API_BASE_URL}/district/list`); // 示例 URL
    return res.data.data; 
};

// 需求 2: 获取板块列表
const getPrecinctList = async (): Promise<Precinct[]> => {
    const res = await axios.get(`${API_BASE_URL}/precinct/list`);
    return res.data.data;
};

// 需求 5: 获取板块地理数据
const getPrecinctGeo = async (districtCode: string): Promise<PrecinctGeoItem[]> => {
    const res = await axios.get(`${API_BASE_URL}/precinct-geo`, {
        params: { district_code: districtCode }
    });
    return res.data.data;
};


// ======== 地图逻辑 ========
const { mapInstance, initMap, destroyMap, isMapReady } = useBaiduMap();
const MAP_CONTAINER_ID = 'precinct-map-container';

let mapVglView: any = null;
let polygonLayer: any = null;
let pointLayer: any = null;
let textLayer: any = null;

const initLayers = () => {
    if (!mapVglView) return;

    polygonLayer = new PolygonLayer({
        enablePicked: true,
        selectedColor: 'rgba(65, 91, 255, 0.6)',
        fillColor: 'rgba(75, 93, 251, 0.3)',
        lineColor: 'rgba(200, 0, 0, 1)',
        lineWidth: 2,
        // 【新增】点击事件配置
        onClick: (e: any) => {
            // e.dataItem 包含了我们在 setData 时传入的 properties
            if (e && e.dataItem && e.dataItem.properties && e.dataItem.properties.code) {
                console.log('地图点击选中:', e.dataItem.properties);
                handleCopyCode(e.dataItem.properties.code);
            }
        }
    });

    pointLayer = new PointLayer({
        blend: 'lighter',
        size: 8,
        color: 'rgba(255, 100, 50, 0.9)'
    });

    textLayer = new TextLayer({
        color: '#000000ff',
        fontSize: 16,
        flat: true,
        offset: [0, -10],
        collides: false,
        enablePicked: false
    });

    mapVglView.addLayer(polygonLayer);
    mapVglView.addLayer(pointLayer);
    mapVglView.addLayer(textLayer);
};

// 【新增】根据当前选中的所有区，重新渲染地图
const renderActiveLayers = () => {
    let finalPolygons: any[] = [];
    let finalPoints: any[] = [];
    let finalTexts: any[] = [];

    // 遍历当前选中的每一个行政区 Code
    activeDistrictCodes.value.forEach(districtCode => {
        // 1. 获取该区的地理数据
        const geoList = geoDataCache.get(districtCode);
        if (!geoList) return;

        // 2. 获取该区的列表数据 (为了拿到 code)
        const districtNode = treeData.value.find(d => d.code === districtCode);
        const subList = districtNode?.children || [];

        // 3. 遍历地理数据并合并
        geoList.forEach((geoItem: any) => {
            // 【关键】通过名称匹配，找到对应的 code
            const matchedSub = subList.find(sub => sub.name === geoItem.name);
            const code = matchedSub ? matchedSub.code : undefined;

            // 构建 Polygon 数据
            finalPolygons.push({
                geometry: geoItem.geometry,
                properties: {
                    name: geoItem.name,
                    code: code // 注入 code 供地图点击使用
                }
            });

            // 构建 Point 和 Text 数据
            if (geoItem.center_point) {
                finalPoints.push({
                    geometry: geoItem.center_point,
                    properties: { name: geoItem.name }
                });
                finalTexts.push({
                    geometry: geoItem.center_point,
                    properties: { text: geoItem.name }
                });
            }
        });
    });

    // 清空或更新数据
    if (finalPolygons.length === 0) {
        if (polygonLayer) polygonLayer.setData([]);
        if (pointLayer) pointLayer.setData([]);
        if (textLayer) textLayer.setData([]);
        return;
    }

    if (polygonLayer) polygonLayer.setData(finalPolygons);
    if (pointLayer) pointLayer.setData(finalPoints);
    if (textLayer) textLayer.setData(finalTexts);
};

// ======== 列表逻辑 ========
const treeData = ref<DistrictNode[]>([]);
const isListLoading = ref(false);

// 【新增】用于存储当前选中的行政区 code 集合 (支持多选)
const activeDistrictCodes = ref<Set<string>>(new Set());
// 【新增】地理数据缓存池，key 为 district_code，value 为该区的板块数据列表
const geoDataCache = new Map<string, any[]>();

// 需求 3: 加载并归类数据
const loadAllData = async () => {
    isListLoading.value = true;
    try {
        // 并发请求行政区和板块数据
        const [districts, precincts] = await Promise.all([
            getDistrictList(),
            getPrecinctList()
        ]);
        console.log('行政区数据:', districts);
        console.log('板块数据:', precincts);
        // 1. 创建行政区 Map，方便快速查找
        const districtMap = new Map<string, DistrictNode>();
        districts.forEach(d => {
            districtMap.set(d.code, { ...d, children: [] });
        });

        // 2. 临时存放找不到归属的板块
        const otherPrecincts: Precinct[] = [];

        // 3. 遍历板块，归类
        precincts.forEach(p => {
            if (p.district_code && districtMap.has(p.district_code)) {
                const parent = districtMap.get(p.district_code);
                parent!.children?.push(p);
            } else {
                // 放入"其他"
                otherPrecincts.push(p);
            }
        });

        // 4. 组装最终列表
        const resultList = Array.from(districtMap.values());

        // 需求 3: 如果有“其他”数据，追加到列表末尾
        if (otherPrecincts.length > 0) {
            resultList.push({
                code: 'OTHER_AREA', // 特殊编码
                name: '其他',
                children: otherPrecincts
            });
        }

        treeData.value = resultList;

    } catch (error) {
        console.error("数据加载失败", error);
    } finally {
        isListLoading.value = false;
    }
};

// ======== 事件处理 ========

// 【修改】一级标题点击获取地理数据（支持多选、显隐、缓存）
const handleLevel1Click = async (district: DistrictNode) => {
    const code = district.code;

    // 特殊处理 'OTHER_AREA' (因为没有真实API对应)
    if (code === 'OTHER_AREA') {
        alert('“其他”分类下为无归属板块，暂无统一地理视图');
        return;
    }

    // A. 如果已经选中，则取消选中 (隐藏)
    if (activeDistrictCodes.value.has(code)) {
        activeDistrictCodes.value.delete(code);
        renderActiveLayers();
        return; 
    }

    // B. 如果未选中，则添加选中 (显示)
    activeDistrictCodes.value.add(code);

    // C. 检查缓存
    if (geoDataCache.has(code)) {
        renderActiveLayers();
    } else {
        try {
            // 调用接口获取数据
            const geoList = await getPrecinctGeo(code);
            
            // 存入缓存 (注意：接口可能返回 null，需处理为空数组)
            geoDataCache.set(code, geoList || []);
            
            // 渲染
            renderActiveLayers();

            // (可选) 首次加载该区时，自动定位视图到该区第一个点
            if (geoList && geoList.length > 0 && geoList[0].center_point) {
                const [lng, lat] = geoList[0].center_point.coordinates;
                mapInstance.value.setCenter(new BMapGL.Point(lng, lat));
                // 稍微调大一点缩放，看清街道
                mapInstance.value.setZoom(13);
            }
        } catch (error) {
            console.error('获取板块地理数据失败', error);
            // 失败时回滚状态
            activeDistrictCodes.value.delete(code);
            showMessage('数据加载失败', 'error');
        }
    }
};

const handleLevel2Click = (item: Precinct) => {
    console.log('点击板块:', item.name);
    // 这里可以添加定位到单个板块的逻辑
};

// 【新增】自定义轻量级消息提示函数（无需引入 UI 库，自动消失）
const showMessage = (msg: string, type: 'success' | 'error' = 'success') => {
    const div = document.createElement('div');
    div.innerText = msg;
    // 设置样式：顶部居中、美观的配色、过渡动画
    div.style.cssText = `
        position: fixed;
        top: 20px;
        left: 50%;
        transform: translateX(-50%);
        background-color: ${type === 'success' ? '#f6ffed' : '#fff2f0'};
        border: 1px solid ${type === 'success' ? '#b7eb8f' : '#ffccc7'};
        color: ${type === 'success' ? '#52c41a' : '#ff4d4f'};
        padding: 10px 20px;
        border-radius: 4px;
        box-shadow: 0 2px 8px rgba(0,0,0,0.15);
        z-index: 9999;
        font-size: 14px;
        pointer-events: none; /* 让鼠标可以穿透提示框继续点击下面的按钮 */
        opacity: 1;
        transition: all 0.3s;
    `;
    document.body.appendChild(div);

    // 1.5秒后淡出并移除 DOM
    setTimeout(() => {
        div.style.opacity = '0';
        div.style.top = '0px';
        setTimeout(() => {
            if (document.body.contains(div)) {
                document.body.removeChild(div);
            }
        }, 300);
    }, 1500);
};

// 【修改】重写复制逻辑
const handleCopyCode = async (code: string) => {
    try {
        await navigator.clipboard.writeText(code);
        // 原来的 alert 会阻断操作，改为使用自定义提示
        showMessage(`复制成功: ${code}`, 'success'); 
    } catch (err) {
        console.error('复制失败', err);
        showMessage('复制失败，请手动复制', 'error');
    }
};
// ======== 生命周期 ========
onMounted(async () => {
    loadAllData();

    await initMap(MAP_CONTAINER_ID, {
        center: { lng: 121.480540, lat: 31.235929 },
        zoom: 11,
        enableScrollWheelZoom: true
    }).catch(err => console.error(err));

    if (isMapReady.value && mapInstance.value) {    
        mapVglView = new MapVglView({ map: mapInstance.value });
        initLayers();
    }
});

onUnmounted(() => {
    destroyMap();
    if (mapVglView) {
        mapVglView.destroy();
        mapVglView = null;
        polygonLayer = null;
        pointLayer = null;
        textLayer = null;
    }
});
</script>

<template>
   <div class="visual-layout">
        <aside class="left-panel">
            <div class="panel-header">
                <h2>板块(Precinct)可视化</h2>
            </div>
            
            <div class="panel-content scroller">
                <div v-if="isListLoading" class="loading-tips">加载行政区与板块数据...</div>
                
                <ul v-else class="tree-root">
                    <li v-for="district in treeData" :key="district.code" class="tree-node">
                        <!-- 一级：行政区 -->
                        <div 
                            class="node-content level-1" 
                            @click="handleLevel1Click(district)"
                        >
                            <span class="label">{{ district.name }}</span>
                            <!-- 如果是"其他"，不显示code或者显示特定样式 -->
                            <span v-if="district.code !== 'OTHER_AREA'" class="level1-code">
                                {{ district.code }}
                            </span>
                        </div>

                        <!-- 二级：板块 -->
                        <ul class="sub-tree">
                            <li v-if="!district.children?.length" class="empty-tips">
                                无板块数据
                            </li>
                            <li 
                                v-for="sub in district.children" 
                                :key="sub.code" 
                                class="sub-node"
                                @click.stop="handleLevel2Click(sub)"
                            >
                                <div class="node-content level-2">
                                    <div class="text-info">
                                        <div class="sub-name">{{ sub.name }}</div>
                                        <div class="sub-code">{{ sub.code }}</div>
                                    </div>
                                    <button class="copy-btn" @click.stop="handleCopyCode(sub.code)">
                                        复制
                                    </button>
                                </div>
                            </li>
                        </ul>
                    </li>
                </ul>
            </div>
        </aside>

        <main class="right-map">
            <div :id="MAP_CONTAINER_ID" class="map-container"></div>
        </main>
    </div>
</template>

<style scoped lang="scss">
// 样式完全复用参考页面，保持一致性
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
        
        .panel-header {
            padding: 20px;
            border-bottom: 1px solid #f0f0f0;
            h2 {
                font-size: 18px;
                font-weight: 600;
                color: #333;
                margin: 0;
            }
        }

        .panel-content {
            flex: 1;
            overflow-y: auto;
            padding: 10px 0;

            &::-webkit-scrollbar { width: 6px; }
            &::-webkit-scrollbar-thumb { background-color: #ccc; border-radius: 3px; }
        }
    }

    .tree-root { list-style: none; padding: 0; margin: 0; }

    .node-content {
        padding: 12px 20px;
        cursor: pointer;
        display: flex;
        align-items: center;
        transition: background-color 0.2s;
        user-select: none;

        &:hover { background-color: #f5f7fa; }

        &.level-1 {
            font-weight: 600;
            color: #333;
            background-color: #fff;
            border-bottom: 1px solid #f5f5f5;
            justify-content: space-between; 

            .label { flex: 1; margin-right: 10px; }
            .level1-code {
                font-weight: normal;
                font-size: 12px;
                color: #999;
                font-family: monospace;
                min-width: 60px; 
                text-align: right;
            }
        }

        &.level-2 {
            padding-left: 40px;
            padding-right: 20px; 
            display: flex;
            justify-content: space-between; 
            align-items: center; 
            min-height: 50px; 
            padding-top: 8px;
            padding-bottom: 8px;
            color: #666;
            font-size: 14px;
            border-left: 3px solid transparent;

            &:hover {
                color: #1890ff;
                background-color: #e6f7ff;
                border-left-color: #1890ff;
            }

            .text-info {
                display: flex;
                flex-direction: column;
                flex: 1;
                margin-right: 10px;
                
                .sub-name { font-weight: 500; margin-bottom: 4px; line-height: 1.4; }
                .sub-code { font-size: 12px; color: #999; font-family: monospace; word-break: break-all; }
            }

            .copy-btn {
                flex-shrink: 0;
                border: 1px solid #d9d9d9;
                background-color: #fff;
                color: #666;
                font-size: 12px;
                padding: 2px 8px;
                border-radius: 4px;
                cursor: pointer;
                transition: all 0.3s;
                min-width: 50px;
                text-align: center;

                &:hover { color: #1890ff; border-color: #1890ff; background-color: #fff; }
                &:active { background-color: #f0f5ff; }
            }
        }
    }

    .sub-tree {
        list-style: none;
        padding: 0;
        margin: 0;
        background-color: #fafafa; 
    }

    .empty-tips { padding: 10px 0 10px 40px; color: #999; font-size: 12px; }
    .loading-tips { text-align: center; color: #999; padding: 20px; }

    .right-map {
        flex: 1;
        position: relative;
        background-color: #f0f2f5;
        .map-container { width: 100%; height: 100%; :deep(.anchorBL) { display: none; } }
    }
}
</style>
