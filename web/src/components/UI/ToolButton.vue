<template>
  <button
    class="tool_btn"
    :class="{ is_danger: danger, is_disabled: disabled }"
    :disabled="disabled"
    type="button"
    @click="$emit('click', $event)"
  >
    <AppIcon :name="icon" :size="21" />
    <span class="tool_btn__label">{{ label }}</span>
  </button>
</template>

<script>
import AppIcon from "./AppIcon.vue";
export default {
  name: "ToolButton",
  components: { AppIcon },
  emits: ["click"],
  props: {
    icon: { type: String, required: true },
    label: { type: String, required: true },
    danger: { type: Boolean, default: false },
    disabled: { type: Boolean, default: false },
  },
};
</script>

<style lang="less" scoped>
.tool_btn {
  width: 60px;
  height: 54px;
  padding: 5px 2px;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  gap: 3px;
  border: none;
  border-radius: 10px;
  background: transparent;
  color: var(--nav-fg);
  font-family: inherit;
  cursor: pointer;
  transition: background 0.15s, color 0.15s;

  &__label {
    font-size: 11px;
    line-height: 1;
    white-space: nowrap;
  }

  &:hover {
    background: var(--hover-bg);
    color: var(--hover-fg);
  }

  &:active {
    background: var(--active-bg);
  }

  &.is_danger:hover {
    background: var(--danger-bg);
    color: var(--danger-fg);
  }

  &.is_disabled {
    opacity: 0.35;
    cursor: not-allowed;
    &:hover {
      background: transparent;
    }
  }
}
</style>
