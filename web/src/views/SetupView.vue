<template>
  <div class="setup">
    <div class="setup__panel">
      <div class="setup_row">
        <div class="setup_row__text">
          <span class="setup_row__name">键盘发声</span>
          <span class="setup_row__desc">总开关，关掉之后按键不再出声</span>
        </div>
        <el-switch
          v-model="store.state.switch_state.keyboard_flag"
          @change="sw"
        />
      </div>

      <div class="setup_row">
        <div class="setup_row__text">
          <span class="setup_row__name">开机自启</span>
          <span class="setup_row__desc">登录桌面后自动把 KeySound 拉起来</span>
        </div>
        <el-switch v-model="store.state.switch_state.auto_run" @change="sw" />
      </div>

      <div class="setup_row">
        <div class="setup_row__text">
          <span class="setup_row__name">只允许一个音效播放</span>
          <span class="setup_row__desc">连按时打断前一个音，而不是叠在一起</span>
        </div>
        <el-switch v-model="store.state.switch_state.single_flag" @change="sw" />
      </div>

      <div class="setup_row">
        <div class="setup_row__text">
          <span class="setup_row__name">停止播放快捷键</span>
          <span class="setup_row__desc">
            按一下就掐掉正在响的音效。录制时按 Esc 取消
          </span>
        </div>
        <div class="hotkey">
          <button
            class="hotkey__value"
            :class="{ is_recording: recording }"
            type="button"
            @click="startRecord"
          >
            {{ recording ? "按下快捷键…" : store.state.switch_state.stop_hotkey || "未设置" }}
          </button>
          <el-button
            size="small"
            :disabled="recording || !store.state.switch_state.stop_hotkey"
            @click="clearHotkey"
          >
            清除
          </el-button>
        </div>
      </div>

      <template v-if="vmic.supported">
        <div class="setup_row">
          <div class="setup_row__text">
            <span class="setup_row__name">虚拟麦克风输出</span>
            <span class="setup_row__desc">
              把音效混进麦克风信号，语音软件里的其他人也能听到。需要管理员权限，开关时会短暂中断系统音频
            </span>
            <span v-if="vmicHint" class="setup_row__desc setup_row__warn">{{ vmicHint }}</span>
          </div>
          <el-switch
            v-model="store.state.switch_state.virtual_mic"
            :loading="vmicBusy"
            @change="toggleVirtualMic"
          />
        </div>

        <div class="setup_row">
          <div class="setup_row__text">
            <span class="setup_row__name">麦克风</span>
            <span class="setup_row__desc">
              音效会混进这只麦克风的信号里，语音软件里不用换设备。开关打开时不能改
            </span>
          </div>
          <el-select
            placeholder="请选择"
            style="width: 220px"
            :disabled="vmicBusy || store.state.switch_state.virtual_mic"
            v-model="store.state.switch_state.virtual_mic_device"
            @change="sw"
          >
            <el-option
              v-for="device in captureDevices"
              :key="device.id"
              :label="device.name + (device.active ? '' : '（未连接）')"
              :value="device.id"
            ></el-option>
          </el-select>
        </div>

        <div class="setup_row">
          <div class="setup_row__text">
            <span class="setup_row__name">挂载槽位</span>
            <span class="setup_row__desc">
              离硬件由近到远是 EFX → MFX → SFX。语音软件用 RAW 模式打开麦克风时只有靠前的槽位还生效，
              听不到就往前换一个
            </span>
          </div>
          <el-select
            placeholder="请选择"
            style="width: 220px"
            :disabled="vmicBusy || store.state.switch_state.virtual_mic"
            v-model="store.state.switch_state.virtual_mic_slot"
            @change="sw"
          >
            <el-option label="EFX（端点效果，推荐）" value="efx"></el-option>
            <el-option label="MFX（模式效果）" value="mfx"></el-option>
            <el-option label="SFX（流效果）" value="sfx"></el-option>
          </el-select>
        </div>

        <div class="setup_row">
          <div class="setup_row__text">
            <span class="setup_row__name">麦克风那一路的音量</span>
            <span class="setup_row__desc">只影响别人听到的音效大小，不影响你自己听到的</span>
          </div>
          <el-slider
            style="width: 220px"
            :min="0"
            :max="200"
            v-model="store.state.switch_state.virtual_mic_volume"
            @change="sw"
          />
        </div>
      </template>

      <div class="setup_row">
        <div class="setup_row__text">
          <span class="setup_row__name">字体</span>
        </div>
        <el-select
          placeholder="请选择"
          style="width: 150px"
          v-model="store.state.switch_state.font"
          @change="sw"
        >
          <el-option label="微软雅黑" value="微软雅黑"></el-option>
        </el-select>
      </div>

      <div class="setup_row">
        <div class="setup_row__text">
          <span class="setup_row__name">主题</span>
          <span class="setup_row__desc">切回“默认”会重新加载界面</span>
        </div>
        <el-select
          placeholder="请选择"
          style="width: 150px"
          v-model="store.state.switch_state.theme"
          @change="sw_1"
        >
          <el-option label="默认（浅色）" value="默认"></el-option>
          <el-option label="深色" value="深色"></el-option>
        </el-select>
      </div>
    </div>
  </div>
</template>

<script>
import { computed, onActivated, onDeactivated, onMounted, onUnmounted, ref } from "vue";
import { useStore } from "vuex";
import { ElMessage } from "element-plus";

// 能当快捷键主键的键，名字要和 utils/keyfilter.py 里的一致
const MAIN_KEYS = {
  Space: "Space",
  Backquote: "~",
  Minus: "-",
  Equal: "=",
  Home: "Home",
  End: "End",
  Insert: "Ins",
  Delete: "Del",
  PageUp: "PgUp",
  PageDown: "PgDn",
  ArrowUp: "↑",
  ArrowDown: "↓",
  ArrowLeft: "←",
  ArrowRight: "→",
};

// 修饰键单独按不算，要等一个主键
const mainKeyName = (event) => {
  const code = event.code || "";
  if (MAIN_KEYS[code]) return MAIN_KEYS[code];
  if (/^Key[A-Z]$/.test(code)) return code.slice(3);
  if (/^Digit[0-9]$/.test(code)) return code.slice(5);
  if (/^F([1-9]|1[0-2])$/.test(code)) return code;
  return "";
};

export default {
  setup() {
    const store = useStore();
    const sw = () => {
      pywebview.api.update_all_switch_state(store.state.switch_state);
    };

    const recording = ref(false);

    const stopRecord = () => {
      if (!recording.value) return;
      recording.value = false;
      window.removeEventListener("keydown", onKeydown, true);
      // 录完了，按键该出声了
      pywebview.api.set_recording_hotkey(false);
    };

    const onKeydown = (event) => {
      // 别让按键顺着冒泡去播音效、也别触发浏览器自己的快捷键
      event.preventDefault();
      event.stopPropagation();
      if (event.code === "Escape") {
        stopRecord();
        return;
      }
      const main = mainKeyName(event);
      if (!main) return;
      const parts = [];
      if (event.ctrlKey) parts.push("Ctrl");
      if (event.altKey) parts.push("Alt");
      if (event.shiftKey) parts.push("Shift");
      if (event.metaKey) parts.push("Win");
      parts.push(main);
      store.state.switch_state.stop_hotkey = parts.join("+");
      stopRecord();
      sw();
    };

    const startRecord = () => {
      if (recording.value) return;
      recording.value = true;
      // 录制期间 Python 那边先别处理按键，不然按什么都在出声
      pywebview.api.set_recording_hotkey(true);
      window.addEventListener("keydown", onKeydown, true);
    };

    const clearHotkey = () => {
      store.state.switch_state.stop_hotkey = "";
      sw();
    };

    // 录制中切走页面或关掉窗口，得把监听和那个标记收回来
    onDeactivated(stopRecord);
    onUnmounted(stopRecord);

    // 虚拟麦克风开关整块跟着 virtual_mic_status().supported 走
    const vmic = ref({ supported: false });
    const captureDevices = ref([]);
    const vmicBusy = ref(false);

    const apiReady = () =>
      new Promise((resolve) => {
        if (window.pywebview && window.pywebview.api) {
          resolve();
          return;
        }
        window.addEventListener("pywebviewready", () => resolve(), { once: true });
      });

    // 开着的状态不能只信 config：注册表可能被音频驱动重装冲掉了，每次进页面现查一次
    const refreshVmic = async () => {
      await apiReady();
      const status = await pywebview.api.virtual_mic_status();
      vmic.value = status || { supported: false };
      if (!vmic.value.supported) return;
      store.state.switch_state.virtual_mic = !!status.enabled;
      captureDevices.value = (await pywebview.api.list_capture_devices()) || [];
      if (!store.state.switch_state.virtual_mic_device) {
        const fallback =
          captureDevices.value.find((d) => d.default) ||
          captureDevices.value.find((d) => d.active);
        if (fallback) store.state.switch_state.virtual_mic_device = fallback.id;
      }
    };

    const vmicHint = computed(() => {
      const status = vmic.value;
      if (!status.supported) return "";
      if (!status.tool_ready)
        return "还缺 KeySoundApo.dll / vmic_setup.exe，按 README 编出来放到 KeySound 目录";
      if (!status.decoder_ready) return "还缺 miniaudio，先跑一遍 pip install -r requirements.txt";
      if (status.needs_repair)
        return "注册表里的配置不见了（音频驱动重装或系统更新会冲掉），关掉再打开一次就能恢复";
      if (status.enabled && !status.apo_running)
        return "还没检测到 APO 被加载，打开语音软件的麦克风测试让采集流跑起来再看";
      if (status.enabled && status.apo_running)
        return `已生效，当前格式 ${status.sample_rate} Hz / ${status.channels} 声道`;
      return "";
    });

    const toggleVirtualMic = async (value) => {
      vmicBusy.value = true;
      try {
        let result;
        if (value) {
          const device = store.state.switch_state.virtual_mic_device;
          if (!device) {
            store.state.switch_state.virtual_mic = false;
            ElMessage({ message: "先选一只麦克风", type: "warning" });
            return;
          }
          result = await pywebview.api.enable_virtual_mic(
            device,
            store.state.switch_state.virtual_mic_slot
          );
        } else {
          result = await pywebview.api.disable_virtual_mic();
        }
        if (result && result.ok) {
          ElMessage({ message: result.message || "已更新", type: "success" });
        } else {
          store.state.switch_state.virtual_mic = !value;
          sw();
          ElMessage({ message: (result && result.message) || "操作失败", type: "error" });
        }
      } finally {
        vmicBusy.value = false;
        refreshVmic();
      }
    };

    onMounted(refreshVmic);
    onActivated(refreshVmic);

    const sw_1 = () => {
      pywebview.api.update_all_switch_state(store.state.switch_state);
      // 默认主题就是不注入任何 css，已经注入过的没法单独撤掉，只能刷新页面
      if (store.state.switch_state.theme === "默认") {
        window.location.reload();
      } else {
        pywebview.api.inject_theme();
      }
    };
    return {
      sw,
      store,
      sw_1,
      recording,
      startRecord,
      clearHotkey,
      vmic,
      vmicHint,
      vmicBusy,
      captureDevices,
      toggleVirtualMic,
    };
  },
};
</script>

<style lang="less" scoped>
.setup {
  flex: 1;
  min-width: 0;
  height: 100%;
  padding: 20px 24px;
  overflow: auto;
  background-color: var(--bg);
  color: var(--fg);

  &__panel {
    max-width: 560px;
  }
}

.setup_row {
  min-height: 56px;
  padding: 12px 0;
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 20px;
  border-bottom: 1px solid var(--border);

  &:last-child {
    border-bottom: none;
  }

  &__text {
    display: flex;
    flex-direction: column;
    gap: 4px;
    min-width: 0;
  }

  &__name {
    font-size: 14px;
  }

  &__desc {
    font-size: 12px;
    opacity: 0.55;
  }

  // 状态提示比说明文字更需要被看见，但别喧宾夺主
  &__warn {
    color: var(--nav-active-fg);
    opacity: 0.9;
  }
}

.hotkey {
  display: flex;
  align-items: center;
  gap: 8px;
  flex: none;

  // 看着像个输入框，其实点了就开始录
  &__value {
    width: 150px;
    height: 32px;
    padding: 0 11px;
    border: 1px solid var(--kb-key-border);
    border-radius: 4px;
    background: var(--kb-key-bg);
    color: var(--fg);
    font-family: inherit;
    font-size: 13px;
    text-align: center;
    cursor: pointer;
    transition: border-color 0.15s;

    &:hover {
      border-color: var(--kb-key-border-hover);
    }

    // 浏览器默认的聚焦黑框在这套界面里太重，换成主题色
    &:focus-visible {
      outline: none;
      border-color: var(--nav-active-fg);
    }

    &.is_recording {
      border-color: var(--nav-active-fg);
      color: var(--nav-active-fg);
    }
  }
}
</style>
