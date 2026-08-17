const { defineConfig } = require('@vue/cli-service')

module.exports = defineConfig({
  publicPath: "./",
  // pywebview 加载的是根目录的 ui/index.html，所以直接构建到那里
  outputDir: "../ui",
})
