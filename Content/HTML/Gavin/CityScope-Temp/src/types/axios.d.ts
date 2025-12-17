import axios from 'axios';

declare module 'axios' {

    // 因为在 apiClient.ts 相应拦截器种，对数据做了解包并返回
    // 通过下面的全局类型让 TypeScript 认为返回的数据是解包后的 T 类型而不是 AxiosResponse
    export interface AxiosInstance {
        request<T = any>(config: AxiosRequestConfig): Promise<T>;
        get<T = any>(url: string, config?: AxiosRequestConfig): Promise<T>;
        delete<T = any>(url: string, config?: AxiosRequestConfig): Promise<T>;
        head<T = any>(url: string, config?: AxiosRequestConfig): Promise<T>;
        options<T = any>(url: string, config?: AxiosRequestConfig): Promise<T>;
        post<T = any>(url: string, data?: any, config?: AxiosRequestConfig): Promise<T>;
        put<T = any>(url: string, data?: any, config?: AxiosRequestConfig): Promise<T>;
        patch<T = any>(url: string, data?: any, config?: AxiosRequestConfig): Promise<T>;
    }
}