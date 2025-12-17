export interface BaiduMapConfig {
    center: { lng: number; lat: number }; // 地图中心坐标
    zoom: number; // 地图缩放级别
    enableScrollWheelZoom: boolean; // 是否启用滚轮缩放
}

declare global {
    interface Window {
        BMapGL: any;
        baiduMapInit?: () => void;
    }
}

export {};