<template>
  <div class="menu">
    <router-link class="item" to="/keyboard">
      <AppIcon name="keyboard" :size="22" />
      <span class="item__label">键盘</span>
    </router-link>
    <router-link class="item" to="/setup">
      <AppIcon name="settings" :size="22" />
      <span class="item__label">设置</span>
    </router-link>
    <router-link class="item" to="/about">
      <AppIcon name="info" :size="22" />
      <span class="item__label">关于</span>
    </router-link>
    <!-- 贴在最下面：不管是按键触发还是点列表试听，都能用这个掐掉 -->
    <button class="item item--stop" type="button" title="停止所有正在播放的音效" @click="stop">
      <AppIcon name="stop" :size="22" />
      <span class="item__label">停止</span>
    </button>
  </div>
</template>

<script>
import AppIcon from "@/components/UI/AppIcon.vue";
export default {
  components: { AppIcon },
  setup() {
    const stop = () => {
      pywebview.api.stopSound();
    };
    return { stop };
  },
};
</script>

<style lang="less" scoped>
.menu {
  width: 62px;
  flex: none;
  height: 100%;
  padding: 8px 6px;
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 4px;
  border-right: 1px solid var(--border);

  .item {
    width: 100%;
    padding: 8px 0 7px;
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 4px;
    border-radius: 10px;
    color: var(--nav-fg);
    text-decoration: none;
    cursor: pointer;
    transition: background 0.15s, color 0.15s;

    &__label {
      font-size: 11px;
      line-height: 1;
    }

    &:hover {
      background: var(--hover-bg);
      color: var(--hover-fg);
    }

    &.router-link-exact-active {
      background: var(--nav-active-bg);
      color: var(--nav-active-fg);
    }
  }

  // button 不像 a 那样继承字体和背景，得自己抹平；margin-top 把它顶到最下面
  .item--stop {
    margin-top: auto;
    border: none;
    background: transparent;
    font-family: inherit;

    &:hover {
      background: var(--danger-bg);
      color: var(--danger-fg);
    }
  }
}
</style>
