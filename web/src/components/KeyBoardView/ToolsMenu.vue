<template>
  <div class="toolbar">
    <!-- 左边：当前音效包和播放模式，以及模式各自需要的参数 -->
    <div class="toolbar__fields">
      <div class="field">
        <span class="field__label">音效包</span>
        <el-select
          v-model="store.state.switch_state.choose_sound"
          placeholder="选择音效包"
          class="field__control"
          style="width: 168px"
          @change="update_switch_state_fn"
        >
          <el-option
            v-for="item in store.state.sound_list"
            :key="item"
            :label="item"
            :value="item"
          />
        </el-select>
      </div>

      <div class="field" v-if="store.state.switch_state.choose_sound">
        <span class="field__label">模式</span>
        <el-select
          v-model="store.state.sound_info.mode"
          placeholder="选择模式"
          class="field__control"
          style="width: 108px"
          @change="update_sound_info_fn"
        >
          <el-option
            v-for="item in ['随机', '重复', '指定', '单键随机']"
            :key="item"
            :label="item"
            :value="item"
          />
        </el-select>
      </div>

      <!-- 重复模式：所有键都放这一个音 -->
      <div class="field" v-if="store.state.sound_info.mode == '重复'">
        <span class="field__label">音效</span>
        <el-select
          v-model="store.state.sound_info.repeat_sound"
          placeholder="选择重复音效"
          class="field__control"
          style="width: 150px"
          @change="update_sound_info_fn"
        >
          <el-option
            v-for="item in store.state.sound_info.soundsList"
            :key="item"
            :label="item"
            :value="item"
          />
        </el-select>
      </div>

      <!-- 指定模式：先按一下键盘选中某个键，再给它挑音效。也可以把音效直接拖到键上 -->
      <div class="field" v-if="store.state.sound_info.mode == '指定'">
        <span class="field__label">键位</span>
        <span class="key_chip" :class="{ is_empty: !store.state.choose_key }">
          {{ store.state.choose_key || "按一下键，或把音效拖上去" }}
        </span>
        <el-select
          v-model="store.state.key_sound"
          placeholder="选择音效"
          class="field__control"
          style="width: 150px"
          :disabled="!store.state.choose_key"
          @change="changeCurrentSound"
        >
          <el-option
            v-for="item in store.state.sound_info.soundsList"
            :key="item"
            :label="item"
            :value="item"
          />
        </el-select>
        <ToolButton
          icon="eraser"
          label="清除"
          danger
          :disabled="!store.state.key_sound"
          @click="clearCurrentSound"
        />
      </div>

      <!-- 单键随机模式：只有勾上的键会响，且每次随机 -->
      <div class="field" v-if="store.state.sound_info.mode == '单键随机'">
        <span class="field__label">键位</span>
        <span class="key_chip" :class="{ is_empty: !store.state.choose_key }">
          {{ store.state.choose_key || "按一下要设置的键" }}
        </span>
        <el-checkbox
          v-model="store.state.onekey_flag"
          :disabled="!store.state.choose_key"
          label="这个键随机发声"
          @change="onekey_change"
        />
      </div>
    </div>

    <!-- 右边：音效包和音效文件的增删导入导出 -->
    <div class="toolbar__actions">
      <ToolButton
        icon="music"
        label="添加音效"
        :disabled="!store.state.switch_state.choose_sound"
        @click="addSoundFile"
      />
      <span class="toolbar__divider"></span>
      <ToolButton icon="folderPlus" label="新建包" @click="createsounds" />
      <ToolButton icon="importPack" label="导入包" @click="importSound" />
      <ToolButton
        icon="exportPack"
        label="导出包"
        :disabled="!store.state.switch_state.choose_sound"
        @click="exportSound"
      />
      <ToolButton
        icon="trash"
        label="删除包"
        danger
        :disabled="!store.state.switch_state.choose_sound"
        @click="delSoundInfo"
      />
    </div>
  </div>
</template>

<script>
import { useStore } from "vuex";
import { ElMessage, ElMessageBox } from "element-plus";
import ToolButton from "@/components/UI/ToolButton.vue";
import {
  update_switch_state,
  add_sound,
  create_sound,
  delStyle,
} from "@/utils/pyapi.js";
export default {
  components: { ToolButton },
  setup() {
    const store = useStore();
    // 更新当前音效包配置
    const update_sound_info_fn = () => {
      store.dispatch("updateSoundInfo");
    };

    // 获取当前音效包配置
    const get_sound_info_fn = () => {
      store.dispatch("getSoundInfo");
    };

    // 更新全局配置
    const update_switch_state_fn = () => {
      update_switch_state();
      get_sound_info_fn();
    };

    // 修改当前key的音效
    const changeCurrentSound = (value) => {
      // 修改配置
      let tmp = "";
      store.state.sound_info.assigned_sounds.forEach((element) => {
        if (element.key == store.state.choose_key) {
          tmp = element;
        }
      });
      if (tmp != "") {
        tmp.sound = value;
      } else {
        store.state.sound_info.assigned_sounds.push({
          key: store.state.choose_key,
          sound: value,
        });
      }
      // // 更新配置
      update_sound_info_fn();
    };

    // 单键位开关变更修改音效包配置然后 update
    const onekey_change = (val) => {
      if (store.state.sound_info.single_key) {
        if (val) {
          store.state.sound_info.single_key.push(store.state.choose_key);
        } else {
          store.state.sound_info.single_key.splice(
            store.state.sound_info.single_key.indexOf(store.state.choose_key),
            1
          );
        }
        update_sound_info_fn();
      }
    };

    // 清除当前key的音效
    const clearCurrentSound = () => {
      // 修改配置
      let tmp = "";
      store.state.sound_info.assigned_sounds.forEach((element) => {
        if (element.key == store.state.choose_key) {
          tmp = element;
        }
      });
      if (tmp != "") {
        store.state.sound_info.assigned_sounds.splice(
          store.state.sound_info.assigned_sounds.indexOf(tmp),
          1
        );
      }
      // 更新配置
      update_sound_info_fn();
      // 把选择的音效清空
      store.state.key_sound = "";
    };

    // 添加音效文件
    const addSoundFile = () => {
      add_sound().then((res) => {
        // 从新获取音效包列表
        store.dispatch("getSoundInfo");
      });
    };

    // 新建音效包
    const createsounds = () => {
      ElMessageBox.prompt("输入音效包名", "新建音效包", {
        confirmButtonText: "确定",
        cancelButtonText: "取消",
        inputPattern: /^[^`~!@#$%^&*()_+<>?:"{},.\/;'[\]]*$/,
        inputErrorMessage: "请输出合法的文件名",
      })
        .then(({ value }) => {
          ElMessage({
            type: "success",
            message: `创建音效包 ${value} 成功`,
          });
          // 创建音效包
          create_sound(value).then((res) => {
            // 重新获取音效包列表
            store.dispatch("getSoundList");
            // 把当前选择音效包设置为新创建的
            store.state.switch_state.choose_sound = value;
            update_switch_state_fn();
          });
        })
        .catch(() => {
          ElMessage({
            type: "info",
            message: "取消创建",
          });
        });
    };

    // 删除音效包
    const delSoundInfo = () => {
      ElMessageBox.confirm("此操作将永久删除该音效包, 是否继续?", "提示", {
        confirmButtonText: "确定",
        cancelButtonText: "取消",
        type: "warning",
      })
        .then(() => {
          ElMessage({
            type: "success",
            message: "删除成功!",
          });
          // 删除音效包
          pywebview.api.delSoundInfo(store.state.sound_info.name).then((res) => {
            // 重新获取音效包列表
            store.dispatch("getSoundList");
            // 把当前选择的音效包清空
            store.state.switch_state.choose_sound = "";
            // 清空音效包信息
            store.state.sound_info = {};
            // 更新选择的音效包
            update_switch_state_fn();
          });
          // 样式清空
          delStyle();
        })
        .catch(() => {
          ElMessage({
            type: "info",
            message: "已取消删除",
          });
        });
    };

    // 导出音效包
    const exportSound = () => {
      if (!store.state.switch_state.choose_sound) {
        ElMessage({
          type: "error",
          message: "请先选择音效包",
        });
        return;
      }
      pywebview.api
        .exportSound(store.state.sound_info.name)
        .then((res) => {
          if (res) {
            ElMessage({
              type: "success",
              message: "导出成功",
            });
          } else {
            ElMessage({
              type: "error",
              message: "导出失败",
            });
          }
        })
        .catch((err) => {
          ElMessage({
            type: "warning",
            message: "取消导出",
          });
        });
    };

    // 导入音效包（.bspack，也兼容老版本导出的 .zip）
    const importSound = () => {
      pywebview.api.importSound().then((names) => {
        // null 是在文件框里点了取消，不用提示
        if (!names) {
          return;
        }
        if (names.length) {
          ElMessage({
            type: "success",
            message: `导入成功：${names.join("、")}`,
          });
          // 重新获取音效包列表
          store.dispatch("getSoundList");
        } else {
          ElMessage({
            type: "error",
            message: "导入失败，请确认选的是 .bspack 音效包",
          });
        }
      });
    };

    return {
      store,
      update_switch_state_fn,
      update_sound_info_fn,
      changeCurrentSound,
      onekey_change,
      clearCurrentSound,
      addSoundFile,
      createsounds,
      delSoundInfo,
      exportSound,
      importSound,
    };
  },
};
</script>

<style lang="less" scoped>
.toolbar {
  // 高度交给内容撑（按钮 54px + 上下 padding + 下边框），别写死数字：
  // 之前写 64px，box-sizing 是 border-box，减掉 padding 和 1px 边框只剩 53px，
  // 比按钮矮 1px，按钮底下那行字就被切掉一点
  flex: none;
  padding: 6px 12px;
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 12px;
  border-bottom: 1px solid var(--border);

  &__fields {
    display: flex;
    align-items: center;
    gap: 14px;
    // 控件多的时候先让左边这组收缩，别把右边的按钮挤出可视区
    min-width: 0;
    overflow: hidden;
  }

  &__actions {
    display: flex;
    align-items: center;
    gap: 2px;
    flex: none;
  }

  &__divider {
    width: 1px;
    height: 28px;
    margin: 0 6px;
    background: var(--divider);
  }
}

.field {
  display: flex;
  align-items: center;
  gap: 8px;
  flex: none;

  &__label {
    font-size: 12px;
    opacity: 0.65;
    white-space: nowrap;
  }
}

.key_chip {
  min-width: 42px;
  padding: 4px 10px;
  border-radius: 7px;
  font-size: 13px;
  text-align: center;
  white-space: nowrap;
  background: var(--chip-bg);
  color: var(--chip-fg);

  &.is_empty {
    font-size: 12px;
    opacity: 0.6;
  }
}

// element-plus 的下拉默认会撑满父容器，这里统一交给外面的 style 定宽
.field__control {
  flex: none;
}
</style>
