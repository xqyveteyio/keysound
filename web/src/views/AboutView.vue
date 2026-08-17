<template>
  <div class="about">
    <p>这是一个基于Vue3.0 + pywebview的桌面应用</p>
    <p><a href="https://space.bilibili.com/1870412606" target="_black">by bilibili Bojaka</a></p>
    <p>用于给 键盘 绑定音效</p>
    <p>目前只支持Windows</p>
    <p style="margin-bottom: 50px;">当前版本 2.2</p>
    <!-- <p style="margin-bottom: 50px;">下次更新将在 2023/3/10</p> -->
    
    <p>另外打个广告</p>
    <p>接程序定制开发 接网站搭建 前端 vue 后端 python node java, APP也是可以哒 用webview 就可以开发啦</p>
    <p>也可以 收几个学生 教写程序 网站 爬虫 人工智能 数据库等等</p>
    <p>会的不多 看看有没有你想学的呢</p>

    <p class="tips">父亲病了,家庭坏了 需要钱修补</p>
  </div>
</template>


<style lang="less" scoped>
.about {
  width: 95%;
  height: 100%;
  display: flex;
  flex-direction: column;
  justify-content: center;
  align-items: center;
  color: #fff;
  font-size: 20px;
  .tips{
    position: fixed;
    right: 0;
    bottom: 0;
    font-size: 13px;
  }
  p{
    margin-bottom: 5px;
    &:last-child{
      margin-bottom: 0;
    }
  }
}
</style>
