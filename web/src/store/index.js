import { createStore } from "vuex";
import {
  get_sound_list_test,
  get_sound_info,
  addStyle,
  update_sound_info,
} from "../utils/pyapi";
export default createStore({
  state: {
    // 全局开关
    switch_state: {
      keyboard_flag: true,
      mouse_flag: false,
      break_flag: false,
      single_flag: false,
      now_play: false,
      choose_sound: "",
      auto_run: false,
      font: "",
      theme: "",
    },
    sound_list: [], // 音效包列表
    sound_info: {
      soundsList: [],
    }, // 存放当前选择的音效包信息
    choose_key: "", // 当前选择的按键
    key_sound: "", // 当前选择的按键的音效
    onekey_flag: false, // 单键模式下 键位的开关
    count: 0, // 计数器 （测试用）
  },
  getters: {},
  mutations: {
    setMenu(state, menu) {
      console.log("setMenu", menu);
      state.count += 1;
    },
  },
  actions: {
    // 获取音效包列表
    getSoundList({ commit, state }) {
      state.soundlist = [];
      get_sound_list_test().then((res) => {
        state.sound_list = res;
        console.log("<<actions>>#getSoundList#", state.sound_list);
      });
    },
    // 获取当前选择音效包的配置
    getSoundInfo({ commit, state }) {
      get_sound_info().then((res) => {
        if (res) {
          state.sound_info = res;
          console.log("<<actions>>#getSoundInfo#", state.sound_info);
          addStyle();
        } else {
          state.sound_info = {
            soundsList: [],
          };
        }
      });
    },
    // 更新音效包配置
    updateSoundInfo({ commit, state }) {
      update_sound_info().then((res) => {
        console.log("<<actions>>#updateSoundInfo#", state.sound_info);
        addStyle();
      });
    },
  },
  modules: {},
});
