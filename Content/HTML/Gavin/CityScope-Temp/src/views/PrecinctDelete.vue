<template>
  <div class="delete-page">
    <h3>删除板块</h3>
    <div class="input-group">
      <input 
        v-model.trim="inputCode" 
        type="text" 
        placeholder="请输入要删除的 Code" 
        @keyup.enter="handleDelete"
      />
      <button 
        @click="handleDelete" 
        :disabled="loading || !inputCode"
        class="delete-btn"
      >
        {{ loading ? '删除中...' : '删除' }}
      </button>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref } from 'vue';
// 引入刚才在 geo.ts 中添加的方法
import { deletePrecinct } from '@/api/geoApi'; 

const inputCode = ref<string>('');
const loading = ref<boolean>(false);

const handleDelete = async () => {
  // 简单的非空校验
  if (!inputCode.value) {
    alert('请输入 Code');
    return;
  }

  try {
    loading.value = true;

    // 调用接口
    // 注意：你的 apiClient 拦截器中，成功(code===200)时返回的是 response.data.data
    // 如果业务失败或网络错误，拦截器会弹出 alert 并抛出异常，进入 catch
    const result = await deletePrecinct(inputCode.value);

    // 能执行到这里说明 code === 200
    // 将返回结果弹窗提示
    const resultMsg = typeof result === 'object' ? JSON.stringify(result) : result;
    alert(`删除成功！
接口返回数据: ${resultMsg || '无数据'}`);

    // 可选：删除成功后清空输入框
    inputCode.value = '';

  } catch (error) {
    // 错误已经在 apiClient 的拦截器中通过 alert 提示过了
    // 这里只需要处理 loading 状态或记录日志
    console.error('组件捕获到删除失败:', error);
  } finally {
    loading.value = false;
  }
};
</script>

<style scoped>
.delete-page {
  padding: 20px;
  max-width: 400px;
}

.input-group {
  display: flex;
  gap: 10px;
  margin-top: 10px;
}

input {
  flex: 1;
  padding: 8px;
  border: 1px solid #ccc;
  border-radius: 4px;
}

.delete-btn {
  padding: 8px 16px;
  background-color: #ff4d4f;
  color: white;
  border: none;
  border-radius: 4px;
  cursor: pointer;
}

.delete-btn:disabled {
  background-color: #ffccc7;
  cursor: not-allowed;
}
</style>