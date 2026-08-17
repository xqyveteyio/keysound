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
  width: 95%;
  background-color: #333;
  // display: flex;
  // flex-direction: column;
  // align-items: center;
  // justify-content: end;
  color: #fff;
}
</style>
