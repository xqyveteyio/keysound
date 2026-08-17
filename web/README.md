# web

KeySound 的前端（Vue3 + Element Plus），构建产物要放到项目根目录的 `ui/` 里，pywebview 加载的是 `ui/index.html`。

包管理用 yarn 4（`packageManager` 字段已经写死版本，用 corepack 会自动切到对应版本）：

```
corepack enable
```

## 安装依赖
```
yarn install
```

### 开发模式（热更新）
```
yarn serve
```
开发时要把 `utils/window_util.py` 里 `create_window_` 的 url 换成注释掉的那个 `http://127.0.0.1:8080/`。

### 打包
```
yarn build
```
`vue.config.js` 里把 `outputDir` 设成了 `../ui`，所以构建产物会直接覆盖到根目录的 `ui/`，不用手动拷。

### 其他配置
See [Configuration Reference](https://cli.vuejs.org/config/).
