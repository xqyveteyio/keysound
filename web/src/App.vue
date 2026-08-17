<template>
  <Load v-if="load" />
  <Menu />
  <!-- <router-view></router-view> -->
  <!-- 组件全部缓存 -->
  <router-view v-slot="{ Component }">
    <keep-alive>
      <component :is="Component" />
    </keep-alive>
  </router-view>
</template>

<script>
import Menu from "@/components/Menu.vue";
import Load from "@/components/Loading.vue";
import { ref, onMounted } from "vue";
import { useStore } from "vuex";
import {test_store} from '@/utils/pyapi'
export default {
  name: "App",
  components: {
    Menu,
    Load,
  },
  data() {
    const store = useStore();
    const load = ref(true);
    window.addEventListener("load", () => {
      load.value = false;
      console.log("load");
    });
    const init = () => {
      pywebview.api.get_all_switch_state().then((res) => {
        console.log("初始会接口");
        console.log(res);
        store.state.switch_state = res;
        console.log("store.state.switch_state:", store.state.switch_state);
        // 获取音效包列表
        const getSoundList = () => {
          store.dispatch("getSoundList");
        };
        getSoundList();
        // 获取当前音效包信息
        console.log("当前选择音效包是:", store.state.switch_state.choose_sound);
        store.dispatch("getSoundInfo");
      });
    };
    onMounted(()=>{
      test_store()
      window.addEventListener("pywebviewready", () => {
      // 在这里获取全部配置信息 发送到vuex 以完成初始化
      init();
      // 初始化主题
      pywebview.api.inject_theme()
    });
    })
    
    return {
      load,
    };
  },
};
</script>
<style lang="less">
#app {
  height: 100%;
  // width: 1200px;
  display: flex;
  background-color: #333;
}
</style>
