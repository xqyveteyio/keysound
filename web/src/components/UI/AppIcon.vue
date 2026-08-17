<template>
  <svg
    class="app_icon"
    :width="size"
    :height="size"
    viewBox="0 0 24 24"
    fill="none"
    stroke="currentColor"
    stroke-width="1.7"
    stroke-linecap="round"
    stroke-linejoin="round"
    aria-hidden="true"
  >
    <path v-for="(d, i) in paths" :key="i" :d="d" />
  </svg>
</template>

<script>
import { computed } from "vue";
import { ICONS } from "@/assets/js/icons.js";
export default {
  name: "AppIcon",
  props: {
    name: { type: String, required: true },
    size: { type: [Number, String], default: 22 },
  },
  setup(props) {
    return { paths: computed(() => ICONS[props.name] || []) };
  },
};
</script>

<style lang="less" scoped>
.app_icon {
  display: block;
  flex: none;
}
</style>
