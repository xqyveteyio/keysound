<template>
  <div class="menu">
    <router-link class="item" to="/keyboard"
      ><i class="iconfont icon-jianpan"></i
    ></router-link>
    <!-- <router-link class="item" to="/mouse"
      ><i class="iconfont icon-mouseM"></i
    ></router-link> -->
    <!-- <router-link class="item" to="/sounds">
      <i class="iconfont icon-shengyin"></i>
    </router-link> -->
    <router-link class="item" to="/setup">
      <i class="iconfont icon-shezhi"></i>
    </router-link>
    <router-link class="item" to="/about"
      ><i class="iconfont icon-guanyu_o"></i
    ></router-link>
    <!-- <router-link class="item" to="/vuex"
      ><i class="iconfont icon-guanyu_o"></i
    ></router-link> -->
  </div>
</template>

<script>
import "@/assets/font/iconfont.css";
export default {};
</script>

<style lang="less" scoped>
.menu {
  width: 5%;
  height: 100%;
  // background: red;
  display: flex;
  flex-direction: column;
  // justify-content: center;
  align-items: center;
  border-right: 1px solid #fff;
  .item {
    width: 100%;
    height: 50px;
    text-decoration: none;
    display: flex;
    justify-content: center;
    align-items: center;
    cursor: pointer;
    margin-bottom: 10px;
    &:hover {
      background: #fff;
      i {
        color: #333;
      }
    }
    // border-radius: 10px;
    i {
      font-size: 30px;
      color: #fff;
    }
    transition: all 0.2s;
    &.router-link-exact-active {
      background: #fff;
      i {
        color: #333;
      }
    }
  }
}
</style>
