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
  </div>
</template>

<script>
import AppIcon from "@/components/UI/AppIcon.vue";
export default {
  components: { AppIcon },
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
}
</style>
