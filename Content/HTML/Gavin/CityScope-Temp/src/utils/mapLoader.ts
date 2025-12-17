const BAIDU_AK = 'tNMbkPkAElOYWetTJ7tYEmWev1A7e2Qf';
const CALLBACK = 'baiduMapInit';

let loadingPromise: Promise<void> | null = null;

export function loadBaiduMapScript(): Promise<void> {
    // 如果已经加载完成
    if (window.BMapGL) {
        return Promise.resolve();
    }

    // 如果正在加载中，直接返回同一个 Promise
    // Promise 本身就支持多次 .then()，不需要额外队列
    if (loadingPromise) {
        return loadingPromise;
    }

    loadingPromise = new Promise((resolve, reject) => {
        // 设置全局回调函数
        window[CALLBACK] = () => {
            resolve();
            // 清理全局变量
            delete window[CALLBACK];
        };

        const script = document.createElement('script');
        script.src = `https://api.map.baidu.com/api?v=1.0&type=webgl&ak=${BAIDU_AK}&callback=${CALLBACK}`;
        
        script.onerror = () => {
            // loadingPromise = null; // 失败后允许重试
            delete window[CALLBACK];
            reject(new Error('百度地图脚本加载失败'));
        };
        
        document.head.appendChild(script);
  });

  return loadingPromise;
}