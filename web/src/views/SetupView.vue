<template>
  <div class="setup">
    <p>
      键盘发声
      <el-switch
        v-model="store.state.switch_state.keyboard_flag"
        @change="sw"
      />
    </p>
    <p>
      开机自启
      <el-switch v-model="store.state.switch_state.auto_run" @change="sw" />
    </p>
    <!-- <p>
      打断前音 &lt;打断之前的音效&gt;
      <el-switch v-model="store.state.switch_state.break_flag" @change="sw" />
    </p> -->
    <p>
      只允许一个音效播放
      <el-switch v-model="store.state.switch_state.single_flag" @change="sw" />
    </p>
    <p>
      字体
      <el-select
        placeholder="请选择"
        v-model="store.state.switch_state.font"
        @change="sw"
      >
        <el-option label="微软雅黑" value="微软雅黑"></el-option>
        <!-- <el-option label="像素字体" value="像素字体"></el-option> -->
      </el-select>
    </p>
    <p>
      主题
      <el-select
        placeholder="请选择"
        v-model="store.state.switch_state.theme"
        @change="sw_1"
      >
        <el-option label="默认" value="默认"></el-option>
        <el-option label="白" value="白"></el-option>
      </el-select>
    </p>
  </div>
</template>

<script>
import { useStore } from "vuex";
export default {
  setup() {
    const store = useStore();
    const sw = () => {
      pywebview.api.update_all_switch_state(store.state.switch_state);
    };
    const sw_1 = () => {
      pywebview.api.update_all_switch_state(store.state.switch_state);
      // 如果主题选的是默认 就提示重启生效
      if (store.state.switch_state.theme === "默认") {
        // 刷新页面
        window.location.reload();
      }else{
        pywebview.api.inject_theme()
      }
    };
    return {
      sw,
      store,
      sw_1,
    };
  },
};
</script>

<style lang="less" scoped>
.setup {
  padding: 10px;
  background-color: #333;
  color: #fff;
  p {
    margin-bottom: 10px;
    &:last-child {
      margin-bottom: 0;
    }
  }
}
</style>
