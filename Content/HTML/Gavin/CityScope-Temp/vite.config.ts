import { defineConfig } from 'vite';
import { viteMockServe } from 'vite-plugin-mock'
import vue from '@vitejs/plugin-vue';

export default defineConfig({
    plugins: [
        vue(),
        viteMockServe({
            mockPath: 'mock', // 设置 mock 文件存放的目录
            enable: true // 开发环境开启
        })
    ],
    server: {
        host: '0.0.0.0',
        port: 5173,
        strictPort: true,
        watch: {
            usePolling: true,
            interval: 3000
        },
        hmr: {
            clientPort: 5173
        }
    },
    build: {
        outDir: 'dist',
        assetsDir: 'assets',
        emptyOutDir: true
    },
    resolve: {
        alias: {
            '@': '/src',
        }
    },
});