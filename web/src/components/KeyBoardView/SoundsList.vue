<template>
  <div class="sound_list">
    <div class="sound_list__empty" v-if="!store.state.sound_info.soundsList?.length">
      {{
        store.state.switch_state.choose_sound
          ? "这个音效包还没有音频，点右上角“添加音效”导入 mp3 / wav"
          : "先在左上角选一个音效包"
      }}
    </div>
    <div class="sound_item" v-for="item in store.state.sound_info.soundsList" :key="item">
      <button
        class="sound_item__name"
        type="button"
        draggable="true"
        :title="`拖到键位上绑定，点一下试听 ${item}`"
        @dragstart="onDragStart(item, $event)"
        @click="previewSound(item)"
      >
        {{ item }}
      </button>
      <el-popconfirm
        title="确定要删除这个音效文件?"
        confirm-button-text="删除"
        cancel-button-text="取消"
        @confirm="deleteSound(item)"
        @cancel="cancelDelete"
      >
        <template #reference>
          <button class="sound_item__del" type="button" title="删除">
            <AppIcon name="trash" :size="15" />
          </button>
        </template>
      </el-popconfirm>
    </div>
  </div>
</template>

<script>
import { useStore } from "vuex";
import { ElMessage } from "element-plus";
import AppIcon from "@/components/UI/AppIcon.vue";
export default {
  components: { AppIcon },
  setup() {
    const store = useStore();

    // 试听：点文件名直接放一遍，左侧“停止”可以掐掉
    const previewSound = (sound) => {
      pywebview.api.previewSound(sound);
    };

    // 拖到键盘上松手时，键位从 dataTransfer 里把文件名读走
    const onDragStart = (sound, event) => {
      event.dataTransfer.setData("text/plain", sound);
      event.dataTransfer.effectAllowed = "copy";
    };

    // 删除音效
    const deleteSound = (sound) => {
      // 删除音效
      pywebview.api.delSoundFile(sound).then((res) => {
        // 从新获取音效信息
        store.dispatch("getSoundInfo");
        ElMessage({
          message: "删除成功.",
          type: "success",
        });
      });
    };
    // 取消删除
    const cancelDelete = () => {
      ElMessage({
        message: "取消删除.",
      });
    };
    return {
      store,
      previewSound,
      onDragStart,
      deleteSound,
      cancelDelete,
    };
  },
};
</script>

<style lang="less" scoped>
.sound_list {
  // 键盘那块按比例定高，剩下的富余高度全归这里，音效再多也在自己这块滚。
  // 高度不跟着内容走，所以换音效包不会再把键盘挤高挤矮。
  // 最矮留够两行（8 + 30 + 6 + 30 + 8）
  flex: 1;
  min-height: 82px;
  padding: 8px 12px;
  display: flex;
  flex-wrap: wrap;
  align-content: flex-start;
  gap: 6px;
  overflow: auto;

  // 进度条样式
  &::-webkit-scrollbar {
    width: 6px;
    height: 6px;
  }
  &::-webkit-scrollbar-thumb {
    background: var(--scrollbar);
    border-radius: 3px;
  }
  &::-webkit-scrollbar-track {
    background: transparent;
  }

  &__empty {
    width: 100%;
    padding: 10px 2px;
    font-size: 12px;
    opacity: 0.55;
  }
}

.sound_item {
  max-width: 210px;
  height: 30px;
  padding: 0 4px 0 10px;
  display: flex;
  align-items: center;
  gap: 4px;
  border-radius: 8px;
  font-size: 13px;
  background: var(--item-bg);
  transition: background 0.15s;

  &:hover {
    background: var(--item-hover-bg);
    .sound_item__del {
      opacity: 1;
    }
  }

  &__name {
    max-width: 160px;
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
    // 是个按钮（点一下试听），但要看着还是一段文字
    padding: 0;
    border: none;
    background: transparent;
    color: inherit;
    font-family: inherit;
    font-size: inherit;
    cursor: grab;
  }

  &__del {
    width: 24px;
    height: 24px;
    display: flex;
    align-items: center;
    justify-content: center;
    border: none;
    border-radius: 6px;
    background: transparent;
    color: inherit;
    cursor: pointer;
    opacity: 0;
    transition: opacity 0.15s, background 0.15s, color 0.15s;

    &:hover {
      background: var(--danger-bg);
      color: var(--danger-fg);
    }
  }
}
</style>
