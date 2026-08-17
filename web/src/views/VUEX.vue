<template>
  <div>
    <h1>用于让 几百年没用vuex 的bojak 复习使用的 页面</h1>
    <h2>{{ state.count }}</h2>
    <button @click="count_">+ 1</button>
  </div>
</template>

<script>
import { useStore } from "vuex";
import {toRefs} from 'vue'
export default {
  setup() {
    const store = useStore();
    const state = store.state
    const count_ = ()=>{
      // store.state.count += 1
      store.commit("setMenu", "vuex");
    }
    
    
    return {
      count_,
      state
    };
  },
};
</script>

<style></style>
