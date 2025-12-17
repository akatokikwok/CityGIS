import axios, { AxiosInstance, AxiosResponse } from 'axios'; 

// 统一响应结构
export interface ApiResponse<T> {
    code: number; // 状态码
    message: string; // 消息
    data: T; // 数据
}

// 创建 Axios 实例
const apiClient: AxiosInstance = axios.create({
    baseURL: import.meta.env.VITE_API_BASE_URL, // 使用环境变量配置基础 URL
    timeout: 10000, // 10 秒超时
    headers: {
        'Content-Type': 'application/json',
    }
});

// 请求拦截器（TODO: 目前无拦截实现）
apiClient.interceptors.request.use(
        (config) => {
        // 可以在这里添加认证 token 等
        return config;
    },
    (error) => {
        console.error('Request error:', error);
        return Promise.reject(error);
    }
);

// 响应拦截器
apiClient.interceptors.response.use(
    (response: AxiosResponse<ApiResponse<any>>) => {
        const { code, message, data } = response.data;
       
        if (code === 200) {
            return data;
        }

        // 业务逻辑失败
        alert(`请求失败 ${code}: ${message}`);
        return Promise.reject(new Error(message));
    },
    // HTTP 状态码不是 2xx 时（网络错误，如 404, 500）
    (error) => {
        // 请求取消处理
        if (axios.isCancel(error)) {
            console.log("请求已取消");
            // 返回一个 pending 的 promise，中断后续的 .then() 或 .catch() 链
            return new Promise(() => {});
        }

        if (error.response) {
            const { status } = error.response;
            alert(`HTTP 错误 ${status}`);
        } else if (error.request) {
             console.log('Response data:', error.request);
            alert('网络错误，未收到响应');
        } else {
            alert(`请求错误: ${error.message}`);
        }
        return Promise.reject(error);
    }
);

export default apiClient;