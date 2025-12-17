import {createApp} from 'vue';
import router from '@/routers';
import App from '@/App.vue';

// 引入全局样式入口文件
import '@/assets/styles/index.scss'

const app = createApp(App);
app.use(router);
app.mount('#app');