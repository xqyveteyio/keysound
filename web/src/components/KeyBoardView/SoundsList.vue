<template>
  <div class="sound_list">
    <div
      class="item"
      v-for="item in store.state.sound_info.soundsList"
      :key="item"
    >
      <span>{{ item }}</span>
      <el-popconfirm
        title="确定要删除?"
        @confirm="deleteSound(item)"
        @cancel="cancelDelete"
      >
        <template #reference>
          <span>删除</span>
        </template>
      </el-popconfirm>
    </div>
  </div>
</template>

<script>
import { useStore } from "vuex";
import { ElMessage } from "element-plus";
export default {
  setup() {
    const store = useStore();

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
      deleteSound,
      cancelDelete,
    };
  },
};
</script>

<style lang="less" scoped>
.sound_list {
  height: calc(35% - 50px);
  width: 395px;
  // background-color: slateblue;
  display: flex;
  flex-wrap: wrap;
  overflow: auto;
  padding-left: 5px;
  // 进度条样式
  &::-webkit-scrollbar {
    width: 5px;
    height: 5px;
  }
  &::-webkit-scrollbar-thumb {
    background: #ccc;
    border-radius: 5px;
  }
  &::-webkit-scrollbar-track {
    background: #fff;
  }

  .item {
    width: 190px;
    padding: 5px;
    overflow: hidden;
    display: flex;
    justify-content: space-between;
    align-items: center;
    border-radius: 3px;
    transition: all 0.2s;
    &:hover {
      background-color: #ccc;
      color: #000;
    }
    span {
      // background: slateblue;
      &:first-child {
        width: 90%;
        overflow: hidden;
        text-overflow: ellipsis;
        white-space: nowrap;
      }
      &:last-child {
        cursor: pointer;
        // 不可换行
        white-space: nowrap;
        &:hover {
          color: red;
        }
      }
    }
  }
}
</style>
