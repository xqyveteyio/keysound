<template>
  <div class="mouse_box">
    <div class="mouse">
      <div class="left" ref="left"></div>
      <div class="right" ref="right"></div>
      <div class="center">{{mouse_sort}}</div>
    </div>
  </div>
  <div class="setupmo">
    <div class="box">
      <div class="left_s"></div>
      <div class="right_s"></div>
    </div>
  </div>
</template>

<script>
import {ref}from 'vue'
export default {
  setup(){
    const left = ref(null)
    const right = ref(null)
    const mouse_sort = ref("")
    const mouse_down = (e) => {
      if(e == 'l'){
        // 添加类名
      left.value.classList.add('down')
      }else if(e == 'r'){
        right.value.classList.add('down')
      }
    }
    const mouse_up = (e) => {
      if(e == 'l'){
        // 移除类名
      left.value.classList.remove('down')
      }else if(e == 'r'){
        right.value.classList.remove('down')
      }
    }
    // 把mouse_down和mouse_up挂在window上，方便全局调用
    window.mouse_down = mouse_down
    window.mouse_up = mouse_up
    
    var timer = null
    const mouse_wheel = (e) => {
      clearTimeout(timer)
      console.log(e);
      if(e.deltaY > 0){
        mouse_sort.value = "↓"
        timer = setTimeout(() => {
          mouse_sort.value = ""
        }, 2000);
      }else{
        mouse_sort.value = "↑"
        timer = setTimeout(() => {
          mouse_sort.value = ""
        }, 2000);
      }
    }
    window.addEventListener('wheel', mouse_wheel)
    return{
      left,
      right,
      mouse_down,
      mouse_up,
      mouse_sort,
      mouse_wheel
    }
  }
}
</script>

<style lang="less" scoped>
  .mouse_box{
    width: 30%;
    height: 100%;
    // background: pink;
    padding: 50px 60px 50px 60px;
    .mouse{
      width: 100%;
      height: 100%;
      background: #333;
      border-radius: 35% 35% 35% 35%;
      position: relative;
      overflow: hidden;
      border: 2px #fff solid;
      .left{
        width: 50%;
        height: 47%;
        background: #fff;
        border-radius: 50% 0% 0% 0%;
        border-right: 2px solid #333;
        transition: all 0.1s;
      }
      .right{
        width: 50%;
        height: 47%;
        background: #fff;
        position: absolute;
        top: 0;
        right: 0;
        border-radius: 0% 50% 0% 0%;
        border-left: 2px solid #333;
        transition: all 0.1s;
      }
      .down{
        background: red;
      }
      .center{
        position: absolute;
        width: 25px;
        height: 50px;
        background: #333;
        left: 50%;
        top: 20%;
        transform: translate(-50%, -50%);
        border-radius: 25%;
        color: #fff;
        display: flex;
        justify-content: center;
        align-items: center;
        font-size: 30px;
      }
    }
  }
  .setupmo{
    width: 60%;
    height: 100%;
    display: flex;
    justify-content: center;
    align-items: center;
    .box{
      width: 100%;
      height: 85%;
      background: #333;
      border-radius: 35% 35% 35% 35%;
      position: relative;
      overflow: hidden;
      border: 2px #fff solid;
    }
  }
</style>