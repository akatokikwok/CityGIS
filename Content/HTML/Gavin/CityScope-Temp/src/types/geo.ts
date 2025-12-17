// 定义一级列表（区/县）的数据类型
export interface District {
    id: number;
    code: string;
    name: string;
    city_code: string;
}

// 定义二级列表（街道/镇）的数据类型
export interface Subdistrict {
    id: number;
    code: string;
    name: string;
    district_code: string;
    city_code: string;
}

// 新增：街道地理数据项结构
export interface SubdistrictGeo {
    name: string;
    center_point: {
        type: 'Point';
        coordinates: [number, number];
    };
    geometry: {
        type: 'MultiPolygon' | 'Polygon';
        coordinates: any[];
    };
    properties: any;
}

// 新增：API 返回结构 (根据你的 mock 数据)
export interface GeoResponse {
    code: number;
    message: string;
    count: number;
    data: SubdistrictGeo[];
}

// 定义提交的数据类型（可选，为了类型安全建议加上）
export interface PrecinctPayload {
    name: string;
    district_code: string;
    center_point: any; // 或者具体的 GeoJSON Point 类型
    geometry: any;     // 或者具体的 GeoJSON MultiPolygon 类型
}