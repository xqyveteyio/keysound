<template>
  <div class="keyboard_view">
    <!-- 操作栏 -->
    <ToolsMenu/>
    <!-- 音效列表 -->
    <SoundsList/>
    <!-- 键盘投射 -->
    <KeyBoard />
  </div>
</template>

<script>
import KeyBoard from "@/components/KeyBoardView/KeyBoard.vue";
import ToolsMenu from "@/components/KeyBoardView/ToolsMenu.vue";
import SoundsList from "@/components/KeyBoardView/SoundsList.vue";
import { onActivated } from "vue";
import {addStyle} from '@/utils/pyapi'
export default {
  name: "HomeView",
  components: {
    KeyBoard,
    ToolsMenu,
    SoundsList
  },
  setup() {
    onActivated(()=>{
      addStyle()
    })
    return {
    };
  },
};
</script>

<style lang="less" scoped>
.keyboard_view {
  height: 100%;
  // 侧边栏定宽，剩下的全给键盘页，min-width 用来防止内容把布局撑破
  flex: 1;
  min-width: 0;
  // 工具栏和音效列表按内容高度排，剩下的高度全给键盘，中间不留空档
  display: flex;
  flex-direction: column;
  background-color: var(--bg);
  color: var(--fg);
}
</style>
