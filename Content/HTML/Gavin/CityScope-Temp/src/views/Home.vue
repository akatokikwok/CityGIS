<script setup lang="ts">
import { useRouter } from 'vue-router';

// 1. 定义类型接口，规范数据结构
interface MenuItem {
    label: string;
    routeName: string;
    type?: 'primary' | 'success' | 'warning' | 'danger'; // 用于区分颜色
}

const router = useRouter();

// 2. 核心优化：使用配置数组代替重复的函数
// 这样以后如果要加按钮，只需要在这里加一行数据，不需要动 HTML
const menuItems: MenuItem[] = [
    { label: '查看行政区划', routeName: 'AdminDivVisual', type: 'primary' },
     { label: '修改街道', routeName: 'SubdistrictModify', type: 'warning' },
    { label: '查看板块', routeName: 'PrecinctVisual', type: 'primary' },
    { label: '添加板块', routeName: 'PrecinctAdd', type: 'success' },
    { label: '修改板块', routeName: 'PrecinctModify', type: 'warning' },
    { label: '删除板块', routeName: 'PrecinctDelete', type: 'danger' },
];

// 3. 统一的跳转处理函数
const handleNavigation = (name: string) => {
    router.push({ name });
};
</script>

<template>
  <div class="page-wrapper">
    <div class="menu-card">
      <h1 class="title">管理控制台</h1>
      <div class="button-group">
        <!-- 使用 v-for 循环渲染，代码更简洁 -->
        <button
          v-for="item in menuItems"
          :key="item.routeName"
          :class="['btn', `btn-${item.type || 'primary'}`]"
          @click="handleNavigation(item.routeName)"
        >
          {{ item.label }}
        </button>
      </div>
    </div>
  </div>
</template>

<style scoped lang="scss">
/* 页面整体背景和居中 */
.page-wrapper {
    display: flex;
    justify-content: center;
    align-items: center;
    min-height: 100vh;
    background-color: #f5f7fa; /* 柔和的背景色 */
}

/* 卡片容器风格 */
.menu-card {
    background: white;
    padding: 40px;
    border-radius: 12px;
    box-shadow: 0 4px 20px rgba(0, 0, 0, 0.08);
    width: 100%;
    max-width: 400px; /* 限制最大宽度，防止在大屏上太宽 */
    text-align: center;

    .title {
        margin-bottom: 30px;
        color: #303133;
        font-size: 24px;
        font-weight: 600;
    }
}

/* 按钮组容器：Flex 纵向排列的核心 */
.button-group {
    display: flex;
    flex-direction: column; /* 关键：竖着排 */
    gap: 16px; /* 按钮之间的间距 */
}

/* 按钮基础样式 */
.btn {
    padding: 12px 20px;
    font-size: 16px;
    cursor: pointer;
    border: none;
    border-radius: 8px;
    font-weight: 500;
    transition: all 0.3s ease;
    width: 100%; /* 让按钮撑满卡片宽度 */
    box-shadow: 0 2px 6px rgba(0, 0, 0, 0.1);

    &:active {
        transform: scale(0.98); /* 点击时的微缩效果 */
    }
}

/* 语义化颜色方案 (仿 Element Plus 风格) */
.btn-primary {
    background-color: #409eff;
    color: white;
    &:hover { background-color: #66b1ff; }
}

.btn-success {
    background-color: #67c23a;
    color: white;
    &:hover { background-color: #85ce61; }
}

.btn-warning {
    background-color: #e6a23c;
    color: white;
    &:hover { background-color: #ebb563; }
}

.btn-danger {
    background-color: #f56c6c;
    color: white;
    &:hover { background-color: #f78989; }
}
</style>