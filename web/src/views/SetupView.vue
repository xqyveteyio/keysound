<template>
  <div class="setup">
    <div class="setup__panel">
      <div class="setup_row">
        <div class="setup_row__text">
          <span class="setup_row__name">键盘发声</span>
          <span class="setup_row__desc">总开关，关掉之后按键不再出声</span>
        </div>
        <el-switch
          v-model="store.state.switch_state.keyboard_flag"
          @change="sw"
        />
      </div>

      <div class="setup_row">
        <div class="setup_row__text">
          <span class="setup_row__name">开机自启</span>
          <span class="setup_row__desc">登录桌面后自动把 KeySound 拉起来</span>
        </div>
        <el-switch v-model="store.state.switch_state.auto_run" @change="sw" />
      </div>

      <div class="setup_row">
        <div class="setup_row__text">
          <span class="setup_row__name">只允许一个音效播放</span>
          <span class="setup_row__desc">连按时打断前一个音，而不是叠在一起</span>
        </div>
        <el-switch v-model="store.state.switch_state.single_flag" @change="sw" />
      </div>

      <div class="setup_row">
        <div class="setup_row__text">
          <span class="setup_row__name">字体</span>
        </div>
        <el-select
          placeholder="请选择"
          style="width: 150px"
          v-model="store.state.switch_state.font"
          @change="sw"
        >
          <el-option label="微软雅黑" value="微软雅黑"></el-option>
        </el-select>
      </div>

      <div class="setup_row">
        <div class="setup_row__text">
          <span class="setup_row__name">主题</span>
          <span class="setup_row__desc">切回“默认”会重新加载界面</span>
        </div>
        <el-select
          placeholder="请选择"
          style="width: 150px"
          v-model="store.state.switch_state.theme"
          @change="sw_1"
        >
          <el-option label="默认（浅色）" value="默认"></el-option>
          <el-option label="深色" value="深色"></el-option>
        </el-select>
      </div>
    </div>
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
      // 默认主题就是不注入任何 css，已经注入过的没法单独撤掉，只能刷新页面
      if (store.state.switch_state.theme === "默认") {
        window.location.reload();
      } else {
        pywebview.api.inject_theme();
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
  flex: 1;
  min-width: 0;
  height: 100%;
  padding: 20px 24px;
  overflow: auto;
  background-color: var(--bg);
  color: var(--fg);

  &__panel {
    max-width: 560px;
  }
}

.setup_row {
  min-height: 56px;
  padding: 12px 0;
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 20px;
  border-bottom: 1px solid var(--border);

  &:last-child {
    border-bottom: none;
  }

  &__text {
    display: flex;
    flex-direction: column;
    gap: 4px;
    min-width: 0;
  }

  &__name {
    font-size: 14px;
  }

  &__desc {
    font-size: 12px;
    opacity: 0.55;
  }
}
</style>
