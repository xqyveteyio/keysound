// 存放 python那边export的函数

// 引入vuex
var store = require("../store").default;

// 测试store
export const test_store = () => {
  console.log("test_store");
  console.log("store", store.state);
};


window.addEventListener("pywebviewready", () => {

})


// 获取音效包列表
export  const  get_sound_list_test = () => {
  return pywebview.api.getSoundList()
}
// 更新全局选项 比如开关键盘音效 当前选择的音效包等
export const update_switch_state = () => {
  return pywebview.api.update_all_switch_state(store.state.switch_state);
}
// 更新当前音效包的配置
export const update_sound_info = () => {
  return pywebview.api.updateSoundInfo(store.state.sound_info);
}
// 获取当前音效包的配置
export const get_sound_info = () => {
  return pywebview.api.getSoundInfo(store.state.switch_state.choose_sound);
}
// 添加音效包接口
export const add_sound = () => {
  return pywebview.api.addSoundFile(store.state.sound_info.name)
}
// 新建音效包接口
export const create_sound = (create_sound_name) => {
  return pywebview.api.newSoundInfo(create_sound_name)
}

// 删除所有样式
export const delStyle = () => {
  let keys = document.querySelectorAll(".key");
  keys.forEach((key) => {
    key.classList.remove("keybind");
    key.removeAttribute("alt");
  });
};

// 给音效包里存在的键位添加样式 - 各种模式根据他们的配置文件来添加样式
export const addStyle = () => {
  // 先获取模式
  let mode = store.state.sound_info.mode;
  // 先清楚所有的样式
  let keys = document.querySelectorAll(".key");
  keys.forEach((key) => {
    key.classList.remove("keybind");
    key.removeAttribute("alt");
  });

  if (mode == "指定") {
    // 先获取到所有的键位
    let keys = document.querySelectorAll(".key");
    store.state.sound_info.assigned_sounds.forEach((element) => {
      console.log(element);
      keys.forEach((key) => {
        if (key.innerText == element.key) {
          // 添加类名
          key.classList.add("keybind");
          // 添加alt属性
          key.setAttribute("alt", element.sound);
        }
      });
    });
  } else if (mode == "重复") {
    // 先获取到所有的键位
    let keys = document.querySelectorAll(".key");
    // 给全部键加上 sound_info.repeat_sound
    keys.forEach((key) => {
      // 添加类名
      key.classList.add("keybind");
      // 添加alt属性
      key.setAttribute("alt", store.state.sound_info.repeat_sound);
    });
  } else if (mode == "随机") {
    // 先获取到所有的键位
    let keys = document.querySelectorAll(".key");
    keys.forEach((key) => {
      // 移除类名
      key.classList.remove("keybind");
      // 移除alt属性
      key.removeAttribute("alt");
    });
  } else if (mode == "单键随机") {
    let keys = document.querySelectorAll(".key");
    store.state.sound_info.single_key.forEach((element) => {
      keys.forEach((key) => {
        if (key.innerText == element) {
          // 添加类名
          key.classList.add("keybind");
          // 添加alt属性
          key.setAttribute("alt", "随机");
        }
      });
    });
  }
};