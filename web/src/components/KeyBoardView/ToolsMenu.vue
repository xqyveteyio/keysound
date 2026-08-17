<template>
  <div class="menu">
    <div class="tools">
    <div style="height: 100%">
      <!-- 选择音效包 -->
      <el-select
        v-model="store.state.switch_state.choose_sound"
        class="m-2"
        placeholder="选择音效包"
        size="large"
        @change="update_switch_state_fn"
      >
        <el-option
          v-for="item in store.state.sound_list"
          :key="item"
          :label="item"
          :value="item"
        />
      </el-select>

      <!-- 选择模式 -->
      <el-select
        v-model="store.state.sound_info.mode"
        class="m-2"
        placeholder="选择模式"
        size="large"
        style="width: 120px; margin-left: 10px"
        @change="update_sound_info_fn"
        v-if="store.state.switch_state.choose_sound"
      >
        <el-option
          v-for="item in ['随机', '重复', '指定', '单键随机']"
          :key="item"
          :label="item"
          :value="item"
        />
      </el-select>

      <!-- 选择指定音效包 -->
      <el-select
        v-model="store.state.sound_info.repeat_sound"
        class="m-2"
        placeholder="选择重复音效"
        size="large"
        style="width: 150px; margin-left: 10px"

        v-show="store.state.sound_info.mode == '重复'"
    
        @change="update_sound_info_fn"
      >
        <el-option
          v-for="item in store.state.sound_info.soundsList"
          :key="item"
          :label="item"
          :value="item"
        />
      </el-select>

      <!-- 选择指定音效 -->
      <span v-show="store.state.sound_info.mode == '指定'">
        <span style="margin: 0px 10px">KEY: {{ store.state.choose_key }}</span>
        <el-select
          v-model="store.state.key_sound"
          class="m-2"
          placeholder="选择音效"
          size="large"
          style="width: 150px"
          @change="changeCurrentSound"
        >
          <el-option
            v-for="item in store.state.sound_info.soundsList"
            :key="item"
            :label="item"
            :value="item"
          />
        </el-select>
      </span>

      <!-- 设置单键开关 -->
      <span v-if="store.state.sound_info.mode == '单键随机'">
        <span style="margin: 0px 10px">KEY: {{ store.state.choose_key }}</span>
        <el-checkbox
          style="color: #fff;"
          v-model="store.state.onekey_flag"
          @change="onekey_change"
          label="随机开关"
          size="large"
        />
      </span>

      <!-- 清除按钮 -->
      <el-tooltip
        class="box-item"
        effect="light"
        content="清除键位绑定"
        placement="top"
        v-if="store.state.sound_info.mode == '指定' && store.state.key_sound"
      >
        <el-button
          plain
          style="margin-left: 10px; padding: 0px 10px"
          @click="clearCurrentSound"
        >
          <i style="font-size: 25px" class="iconfont icon-qingchu"></i>
        </el-button>
      </el-tooltip>
      <el-tooltip
        class="box-item"
        effect="light"
        content="导入音效文件"
        placement="top"
        v-if="store.state.switch_state.choose_sound"
      >
        <el-button
          plain
          style="margin-left: 10px; padding: 0px 10px"
          @click="addSoundFile"
        >
          <i style="font-size: 25px" class="iconfont icon-daoru_o"></i>
        </el-button>
      </el-tooltip>

      <el-tooltip
        class="box-item"
        effect="light"
        content="删除音效包"
        placement="top"
        v-if="store.state.switch_state.choose_sound"
      >
        <el-button
          plain
          style="margin-left: 10px; padding: 0px 10px"
          @click="delSoundInfo"
        >
          <i style="font-size: 25px" class="iconfont icon-shanchu"></i>
        </el-button>
      </el-tooltip>
    </div>

    <div style="height: 100%">
      <el-tooltip
        class="box-item"
        effect="light"
        content="新建音效包"
        placement="top"
      >
        <el-button
          plain
          style="margin-left: 10px; padding: 0px 10px"
          @click="createsounds"
        >
          <i style="font-size: 25px" class="iconfont icon-xinjian"></i>
        </el-button>
      </el-tooltip>

      <el-tooltip
        class="box-item"
        effect="light"
        content="导入音效包"
        placement="top"
      >
        <el-button
          plain
          style="margin-left: 10px; padding: 0px 10px"
          @click="importSound"
        >
          <i
            style="
              font-size: 25px;
              transform: rotate(180deg);
              -webkit-transform: rotate(180deg);
            "
            class="iconfont icon-daochu"
          ></i>
        </el-button>
      </el-tooltip>

      <el-tooltip
        class="box-item"
        effect="light"
        content="导出音效包"
        placement="top"
      >
        <el-button
          plain
          style="margin-left: 10px; padding: 0px 10px"
          @click="exportSound"
        >
          <i style="font-size: 25px" class="iconfont icon-daochu"></i>
        </el-button>
      </el-tooltip>

      <!-- 上传云端 -->
      <!-- <el-tooltip
        class="box-item"
        effect="light"
        content="上传创意工坊"
        placement="top"
      >
        <el-button
          plain
          style="margin-left: 10px; padding: 0px 10px"
          @click="postcloud"
        >
          <i style="font-size: 34px" class="iconfont icon-yunshangchuan"></i>
        </el-button>
      </el-tooltip> -->
    </div>
  </div>
</div>
</template>

<script>
import { useStore } from "vuex";
import { reactive, toRefs } from "vue";
import { ElMessage, ElMessageBox } from "element-plus";
import {
  update_switch_state,
  add_sound,
  create_sound,
  addStyle,
  delStyle,
} from "@/utils/pyapi.js";
export default {
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
      // store.dispatch("createSound");
      ElMessageBox.prompt("输入音效包名", "Tip", {
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
            // console.log(res);
            // 重新获取音效包列表
            store.dispatch("getSoundList");
            // 把当前选择的音效包清空
            store.state.switch_state.choose_sound = "";
            // 清空音效包信息
            store.state.sound_info = {};
            // 更新选择的音效包
            update_switch_state_fn()
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
      pywebview.api.exportSound(store.state.sound_info.name).then((res) => {
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
      }).catch((err) => {
        ElMessage({
            type: "warning",
            message: "取消导出",
          });
      });
    };

    // 导入音效包
    const importSound = () => {
      pywebview.api.importSound().then((res) => {
        if (res) {
          ElMessage({
            type: "success",
            message: "导入成功",
          });
          // 重新获取音效包列表
          store.dispatch("getSoundList");
        } else {
          ElMessage({
            type: "error",
            message: "导入失败",
          });
        }
      });
    };
    // 上传到云端
    const postcloud = ()=>{
      if (!store.state.switch_state.choose_sound) {
        ElMessage({
          type: "error",
          message: "请先选择音效包",
        });
        return;
      }
      pywebview.api.upload_sound().then((res) => {
        if (res) {
          ElMessage({
            type: "success",
            message: "上传成功",
          });
        } else {
          ElMessage({
            type: "error",
            message: "上传失败",
          });
        }
      });
    }
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
      postcloud
    };
  },
};
</script>

<style lang="less" scoped>
.menu {
  width: 100%;
  height: 50px;
  // background: red;
  border-bottom: 1px solid #ebeef5;
  padding: 5px 10px;
  .tools{
    width: 100%;
    height: 40px;
    display: flex;
    justify-content: space-between;
    align-items: center;
    overflow: hidden;
  }
}
/deep/.el-checkbox.el-checkbox--large{
  height: auto !important;
}
/deep/.el-button{
  height: 100% !important;
}
</style>
