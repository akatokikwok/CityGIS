import { ref, shallowRef } from "vue";
import type { BaiduMapConfig } from "@/types/baidu-map";
import { loadBaiduMapScript } from "@/utils/mapLoader";

export function useBaiduMap() {
    const mapInstance = shallowRef<any>(null); // 百度地图实例
    const isMapReady = ref(false); // 地图是否初始化完成

    /**
     * 初始化百度地图
     */
    async function initMap(
        containerId: string,
        config: BaiduMapConfig
    ): Promise<void> {
        try {
            await loadBaiduMapScript();

            // 创建地图实例
            mapInstance.value = new window.BMapGL.Map(containerId);
            // 设置中心点和缩放级别
            const centerPoint = new window.BMapGL.Point(
                config.center.lng,
                config.center.lat
            );
            mapInstance.value.centerAndZoom(centerPoint, config.zoom);
            // 是否启用滚轮缩放
            if (config.enableScrollWheelZoom) {
                mapInstance.value.enableScrollWheelZoom();
            }

            isMapReady.value = true;
        } catch (error) {
            console.error("加载百度地图脚本失败:", error);
            throw error;
        }
    }

    /**
     * 销毁百度地图实例
     */
    function destroyMap(): void {
        if (mapInstance.value) {
            mapInstance.value.destroy();
            mapInstance.value = null;
            isMapReady.value = false;
        }
    }

    return {
        mapInstance,
        isMapReady,
        initMap,
        destroyMap
    };
}