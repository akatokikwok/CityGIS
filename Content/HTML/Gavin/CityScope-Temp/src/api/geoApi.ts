import apiClient from '@/api/apiClient';
import type { District, Subdistrict, GeoResponse, PrecinctPayload } from "@/types/geo";

// 获取一级列表 (区)
// 假设 URL 为 /district/list，如果实际是 /subdistrict/list 请修改
export const getDistrictList = () => {
    return apiClient.get<District[]>('/district/list');
};

// 获取二级列表 (街道)
// 通常需要传入 district_code 进行过滤，或者后端一次性返回所有
// 这里假设可以通过 query 参数过滤: /subdistrict/list?district_code=310101
export const getSubdistrictList = (districtCode?: string) => {
    const params = districtCode ? { district_code: districtCode } : {};
    return apiClient.get<Subdistrict[]>('/subdistrict/list', { params });
};

// 新增：获取街道地理围栏数据
export const getSubdistrictGeo = (districtCode: string) => {
    // 假设 apiClient 已经处理了响应拦截，直接返回 response data
    // 如果 apiClient 返回的是 AxiosResponse，请自行调整
    return apiClient.get<GeoResponse>('/subdistrict-geo', { 
        params: { district_code: districtCode } 
    });
};

export const addPrecinct = (data: PrecinctPayload) => {
    return apiClient.post('/precinct/add', data);
};

// 新增：根据 code 获取板块详情
export const getPrecinctByCode = (code: string) => {
    return apiClient.get<any>('/precinct', { 
        params: { code } 
    });
};

// 新增：更新板块数据
// 注意：Payload 类型最好定义一下，这里使用 any 或继承 PrecinctPayload 并添加 code
export const updatePrecinct = (data: any) => {
    return apiClient.post('/precinct/update', data);
};

export const deletePrecinct = (code: string) => {
    return apiClient.post('/precinct/delete', { code });
};

// 1. 获取街道详情 (用于回显数据)
export const getSubdistrictByCode = (code: string) => {
    return apiClient.get<any>('/subdistrict', { 
        params: { code } 
    });
};

// 2. 更新街道数据 (只提交形状和中心点)
// Payload: { code, geometry, center_point }
export const updateSubdistrict = (data: any) => {
    return apiClient.post('/subdistrict/update', data);
};

// 3. 获取参考区域数据的通用接口
// 用于差集/交集计算时，获取其他街道或板块的围栏作为参考
// 假设后端有一个通用的 geo 接口，或者分别调用 /subdistrict-geo 和 /precinct-geo
export const getGeoByCode = (code: string, type: 'subdistrict' | 'precinct') => {
    const url = type === 'subdistrict' ? '/subdistrict-geo' : '/precinct-geo';
    // 这里假设参数名统一为 district_code 或者 code，根据你提供的 precinct 代码，
    // 之前的 getSubdistrictGeo 用的是 district_code，这里需注意后端参数定义。
    // 假设后端获取单个围栏详情都通用 param 'code'
    return apiClient.get<GeoResponse>(url, { 
        params: { code: code } 
    });
};