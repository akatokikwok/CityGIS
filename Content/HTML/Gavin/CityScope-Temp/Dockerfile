FROM --platform=linux/amd64 node:22.21.0-bookworm

ENV TZ=Asia/Shanghai

# 在容器内创建一个工作目录
WORKDIR /usr/src/app

# 先复制 package.json 和 package-lock.json 到工作目录
# 这样可以利用 Docker 的缓存机制，只有在依赖更新时才重新安装
COPY package*.json ./

RUN chown -R node:node /usr/src/app

USER node

# 安装项目依赖
RUN npm install

# 声明服务将要监听的端口
EXPOSE 5173

# 定义容器启动时执行的命令
CMD ["npm", "run", "dev"]