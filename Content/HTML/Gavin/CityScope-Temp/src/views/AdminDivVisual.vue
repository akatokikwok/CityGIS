<script setup lang="ts">
import { onMounted, onUnmounted, ref } from 'vue';
import { useBaiduMap } from '@/composable/useBaiduMap';
import type { District, Subdistrict } from '@/types/geo';
import { getDistrictList, getSubdistrictList, getSubdistrictGeo } from '@/api/geoApi';
import { View as MapVglView, PolygonLayer, PointLayer, TextLayer } from 'mapvgl';

// ======== 地图逻辑相关 ========
// 使用封装好的 hook
const { mapInstance, initMap, destroyMap, isMapReady } = useBaiduMap();
const MAP_CONTAINER_ID = 'visual-map-container';

// MapVGL 相关逻辑
let mapVglView: any = null;
let polygonLayer: any = null;
let pointLayer: any = null;
let textLayer: any = null;

const initLayers = () => {
    if (!mapVglView) return;

    // 1. 平面多边形图层 (PolygonLayer)
    polygonLayer = new PolygonLayer({
        enablePicked: true,
        selectedColor: 'rgba(50, 50, 200, 0.6)',
        fillColor: 'rgba(50, 50, 200, 0.3)',
        lineColor: 'rgba(50, 50, 255, 1)',
        lineWidth: 2,
        // 【新增】点击事件配置
        onClick: (e: any) => {
            console.log(e);
            // e.dataItem 包含了我们在 setData 时传入的 properties
            if (e && e.dataItem && e.dataItem.properties && e.dataItem.properties.code) {
                console.log('地图点击选中:', e.dataItem.properties);
                handleCopyCode(e.dataItem.properties.code);
            }
        }
    });

    // 2. 点图层 (PointLayer)
    pointLayer = new PointLayer({
        blend: 'lighter',
        size: 10,
        color: 'rgba(255, 200, 50, 0.9)' // 黄色高亮点
    });

    // 3. 文字图层 (TextLayer)
    textLayer = new TextLayer({
        color: '#22ffdeff',
        fontSize: 14,
        flat: true, //文字平铺
        offset: [0, -10], // 文字稍微上移
        collides: false, // 是否开启碰撞检测（避免文字重叠）
        enablePicked: false
    });

    // 将图层添加到 View 中
    mapVglView.addLayer(polygonLayer);
    mapVglView.addLayer(pointLayer);
    mapVglView.addLayer(textLayer);
};

// 【修改】根据当前选中的所有区，重新渲染地图
const renderActiveLayers = () => {
    // 准备存放最终给 MapVGL 的数据数组
    let finalPolygons: any[] = [];
    let finalPoints: any[] = [];
    let finalTexts: any[] = [];

    // 遍历当前选中的每一个行政区 Code
    activeDistrictCodes.value.forEach(districtCode => {
        // 1. 获取该区的地理数据 (包含 geometry, name)
        const geoList = geoDataCache.get(districtCode);
        if (!geoList) return;

        // 2. 获取该区的列表数据 (包含 code, name)
        // 我们需要从 treeData 中找到对应的数据来补全 code
        const districtNode = treeData.value.find(d => d.code === districtCode);
        const subList = districtNode?.children || [];

        // 3. 遍历地理数据并合并
        geoList.forEach((geoItem: any) => {
            // 【关键步骤】通过名称匹配，找到对应的 code
            const matchedSub = subList.find(sub => sub.name === geoItem.name);
            const code = matchedSub ? matchedSub.code : undefined;

            // 构建 Polygon 数据
            finalPolygons.push({
                geometry: geoItem.geometry,
                properties: {
                    name: geoItem.name,
                    code: code // 这里成功注入了 code
                }
            });

            // 构建 Point 和 Text 数据 (如果有中心点的话)
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

    if (finalPolygons.length === 0) {
        // 清空地图
        if (polygonLayer) polygonLayer.setData([]);
        if (pointLayer) pointLayer.setData([]);
        if (textLayer) textLayer.setData([]);
        return;
    }

    // 4. 更新图层
    if (polygonLayer) polygonLayer.setData(finalPolygons);
    if (pointLayer) pointLayer.setData(finalPoints);
    if (textLayer) textLayer.setData(finalTexts);
};



// ======== 列表相关 ========
// 1. 修改接口定义：不再需要 expanded 和 loading 状态
interface DistrictNode extends District {
    children?: Subdistrict[]; // 直接包含子集数据
}

const treeData = ref<DistrictNode[]>([]);
const isListLoading = ref(false);

// 【新增】用于存储当前选中的行政区 code 集合 (支持多选)
const activeDistrictCodes = ref<Set<string>>(new Set());
// 【新增】地理数据缓存池，key 为 district_code，value 为该区的街道数据列表
const geoDataCache = new Map<string, any[]>();

// 2. 修改加载逻辑：一次性加载所有层级数据
const loadAllData = async () => {
    isListLoading.value = true;
    try {
        // 第一步：获取一级行政区
        const districts = await getDistrictList();
        
        // 第二步：并发获取所有行政区下的二级街道数据
        // 使用 Promise.all 提高加载速度
        const promises = districts.map(async (district) => {
            try {
                // 获取当前区的街道数据
                const subData = await getSubdistrictList(district.code);
                return {
                    ...district,
                    // 过滤数据（如果后端返回全量数据需保留此 filter，如果是精准返回则不需要）
                    children: subData.filter(sub => sub.district_code === district.code)
                };
            } catch (e) {
                console.error(`加载 ${district.name} 子数据失败`, e);
                return { ...district, children: [] };
            }
        });

        treeData.value = await Promise.all(promises);

    } catch (error) {
        console.error("获取行政区划数据失败", error);
    } finally {
        isListLoading.value = false;
    }
};

// ======== 事件处理 (需求重点) ========

// 3. 一级标题点击事件 (修改版)
const handleLevel1Click = async (district: DistrictNode) => {
    const code = district.code;

    // A. 如果已经选中，则取消选中 (隐藏)
    if (activeDistrictCodes.value.has(code)) {
        activeDistrictCodes.value.delete(code);
        renderActiveLayers(); // 重新渲染剩余的
        return; 
    }

    // B. 如果未选中，则添加选中 (显示)
    activeDistrictCodes.value.add(code);

    // C. 检查缓存，如果有数据直接渲染，没有数据则请求
    if (geoDataCache.has(code)) {
        renderActiveLayers();
    } else {
        try {
            const res = await getSubdistrictGeo(code);
            // 假设 res 即为数组，根据实际情况调整
            const geoList = res || []; 
            
            // 存入缓存
            geoDataCache.set(code, geoList);
            
            // 渲染
            renderActiveLayers();

            // 首次加载该区时，自动定位视图到该区第一个点 (可选体验优化)
            if (geoList.length > 0 && geoList[0].center_point) {
                const [lng, lat] = geoList[0].center_point.coordinates;
                mapInstance.value.setCenter(new BMapGL.Point(lng, lat));
                mapInstance.value.setZoom(12);
            }
        } catch (error) {
            console.error('获取地理数据失败', error);
            // 请求失败需回滚状态
            activeDistrictCodes.value.delete(code); 
            showMessage('数据加载失败', 'error');
        }
    }
};


// 4. 二级标题点击事件 (留空函数)
const handleLevel2Click = (subItem: Subdistrict) => {
    console.log('点击了二级标题:', subItem.name);
    // TODO: 在此处添加二级标题点击后的逻辑
    
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

onMounted(async () => {
    // 初始化加载全部数据
    loadAllData();

    await initMap(MAP_CONTAINER_ID, {
        center: { lng: 121.480540, lat: 31.235929 },
        zoom: 12,
        enableScrollWheelZoom: true
    }).catch(err => {
        console.error('地图初始化失败', err);
    });

    if (isMapReady.value && mapInstance.value) {    
        mapVglView = new MapVglView({
            map: mapInstance.value
        });
        // 初始化图层
        initLayers();
    }
});

// 页面卸载时销毁地图实例，防止内存泄漏
onUnmounted(() => {
    destroyMap();
    if (mapVglView) {
        mapVglView.destroy();
        mapVglView = null;
        // 清理引用
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
                <h2>上海市行政区划</h2>
            </div>
            
            <div class="panel-content scroller">
                <div v-if="isListLoading" class="loading-tips">数据全量加载中...</div>
                
                <ul v-else class="tree-root">
                    <li v-for="district in treeData" :key="district.id" class="tree-node">
                        <!-- 
                           5. 一级菜单项修改：
                           - 移除了 icon-arrow
                           - 移除了 loading spinner
                           - 绑定了 handleLevel1Click
                        -->
                        <div 
                            class="node-content level-1" 
                            @click="handleLevel1Click(district)"
                        >
                            <span class="label">{{ district.name }}</span>
                            <span class="level1-code">{{ district.code }}</span>
                        </div>

                        <!-- 
                           6. 二级菜单列表修改：
                           - 移除了 transition 动画
                           - 移除了 v-if="district.expanded" (始终显示)
                        -->
                        <ul class="sub-tree">
                            <li v-if="!district.children?.length" class="empty-tips">
                                暂无下级数据
                            </li>
                            <li 
                                v-for="sub in district.children" 
                                :key="sub.id" 
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

            &::-webkit-scrollbar {
                width: 6px;
            }
            &::-webkit-scrollbar-thumb {
                background-color: #ccc;
                border-radius: 3px;
            }
        }
    }

    .tree-root {
        list-style: none;
        padding: 0;
        margin: 0;
    }

    .node-content {
        padding: 12px 20px;
        cursor: pointer;
        display: flex;
        align-items: center;
        transition: background-color 0.2s;
        user-select: none;

        &:hover {
            background-color: #f5f7fa;
        }

        /* 一级样式微调：去掉了 justify-content: space-between，因为没有箭头了 */
        &.level-1 {
            font-weight: 600;
            color: #333;
            background-color: #fff;
            border-bottom: 1px solid #f5f5f5;
            
            /* 【新增】让 Name 和 Code 左右分布 */
            justify-content: space-between; 

            .label {
                /* 修改：不需要 flex: 1 强占所有空间，留出一点给右侧 */
                flex: 1; 
                margin-right: 10px;
            }

            /* 【新增】Code 的样式 */
            .level1-code {
                font-weight: normal;
                font-size: 12px;
                color: #999;
                font-family: monospace;
                /* 可选：设置一个最小宽度，确保视觉上的“列”感 */
                min-width: 60px; 
                text-align: right;
            }
        }

        &.level-2 {
            padding-left: 40px;
            padding-right: 20px; 
            /* 使用 Flex 布局：左侧文字，右侧按钮 */
            display: flex;
            justify-content: space-between; 
            align-items: center; 
            
            /* 允许高度被内容撑开，确保能全部显示 */
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

            /* 左侧文字容器：上下排列 */
            .text-info {
                display: flex;
                flex-direction: column;
                flex: 1;
                margin-right: 10px; /* 给按钮留出间距 */
                
                .sub-name {
                    font-weight: 500;
                    margin-bottom: 4px;
                    line-height: 1.4;
                }

                .sub-code {
                    font-size: 12px;
                    color: #999;
                    font-family: monospace;
                    word-break: break-all; /* 确保长数字换行显示，不被遮挡 */
                }
            }

            /* 右侧按钮样式 */
            .copy-btn {
                flex-shrink: 0; /* 防止按钮被挤压 */
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

                &:hover {
                    color: #1890ff;
                    border-color: #1890ff;
                    background-color: #fff;
                }
                
                &:active {
                    background-color: #f0f5ff;
                }
            }
        }
    }

    .sub-tree {
        list-style: none;
        padding: 0;
        margin: 0;
        /* 保持展开的背景色，稍微区分层级 */
        background-color: #fafafa; 
    }

    .empty-tips {
        padding: 10px 0 10px 40px;
        color: #999;
        font-size: 12px;
    }

    .loading-tips {
        text-align: center;
        color: #999;
        padding: 20px;
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