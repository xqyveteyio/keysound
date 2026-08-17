<template>
  <div class="keyboard">
    <div class="keyboard_box" ref="keybox">
      <div class="cell"><div class="key">Esc</div></div>
      <div class="cell"></div>
      <div class="cell"><div class="key">F1</div></div>
      <div class="cell"><div class="key">F2</div></div>
      <div class="cell"><div class="key">F3</div></div>
      <div class="cell"><div class="key">F4</div></div>
      <div class="cell"></div>
      <div class="cell"><div class="key">F5</div></div>
      <div class="cell"><div class="key">F6</div></div>
      <div class="cell"><div class="key">F7</div></div>
      <div class="cell"><div class="key">F8</div></div>
      <div class="cell"></div>
      <div class="cell"><div class="key">F9</div></div>
      <div class="cell"><div class="key">F10</div></div>
      <div class="cell"><div class="key">F11</div></div>
      <div class="cell"><div class="key">F12</div></div>
      <div class="cell"><div class="key">SrcLk</div></div>
      <div class="cell"><div class="key">Pause</div></div>
      <div class="cell"><div class="key">~</div></div>
      <div class="cell"><div class="key">1</div></div>
      <div class="cell"><div class="key">2</div></div>
      <div class="cell"><div class="key">3</div></div>
      <div class="cell"><div class="key">4</div></div>
      <div class="cell"><div class="key">5</div></div>
      <div class="cell"><div class="key">6</div></div>
      <div class="cell"><div class="key">7</div></div>
      <div class="cell"><div class="key">8</div></div>
      <div class="cell"><div class="key">9</div></div>
      <div class="cell"><div class="key">0</div></div>
      <div class="cell"><div class="key">-</div></div>
      <div class="cell"><div class="key">=</div></div>
      <div class="cell backspace"><div class="key">Backspace</div></div>
      <div class="cell"><div class="key">Ins</div></div>
      <div class="cell"><div class="key">Home</div></div>
      <div class="cell"><div class="key">PgUp</div></div>
      <div class="cell tab"><div class="key">Tab</div></div>
      <div class="cell"><div class="key">Q</div></div>
      <div class="cell"><div class="key">W</div></div>
      <div class="cell"><div class="key">E</div></div>
      <div class="cell"><div class="key">R</div></div>
      <div class="cell"><div class="key">T</div></div>
      <div class="cell"><div class="key">Y</div></div>
      <div class="cell"><div class="key">U</div></div>
      <div class="cell"><div class="key">I</div></div>
      <div class="cell"><div class="key">O</div></div>
      <div class="cell"><div class="key">P</div></div>
      <div class="cell"><div class="key">[ {</div></div>
      <div class="cell"><div class="key">] }</div></div>
      <div class="cell"><div class="key">\</div></div>
      <div class="cell"><div class="key">Del</div></div>
      <div class="cell"><div class="key">End</div></div>
      <div class="cell"><div class="key">PgDn</div></div>
      <div class="cell caps"><div class="key">Caps</div></div>
      <div class="cell"><div class="key">A</div></div>
      <div class="cell"><div class="key">S</div></div>
      <div class="cell"><div class="key">D</div></div>
      <div class="cell"><div class="key">F</div></div>
      <div class="cell"><div class="key">G</div></div>
      <div class="cell"><div class="key">H</div></div>
      <div class="cell"><div class="key">J</div></div>
      <div class="cell"><div class="key">K</div></div>
      <div class="cell"><div class="key">L</div></div>
      <div class="cell"><div class="key">; :</div></div>
      <div class="cell"><div class="key">'</div></div>
      <div class="cell enter"><div class="key">Enter</div></div>
      <div class="cell"></div>
      <div class="cell"></div>
      <div class="cell"></div>
      <div class="cell shift-left"><div class="key">L Shift</div></div>
      <div class="cell"><div class="key">Z</div></div>
      <div class="cell"><div class="key">X</div></div>
      <div class="cell"><div class="key">C</div></div>
      <div class="cell"><div class="key">V</div></div>
      <div class="cell"><div class="key">B</div></div>
      <div class="cell"><div class="key">N</div></div>
      <div class="cell"><div class="key">M</div></div>
      <div class="cell"><div class="key">, &lt;</div></div>
      <div class="cell"><div class="key">. ></div></div>
      <div class="cell"><div class="key">/ ?</div></div>
      <div class="cell shift-right"><div class="key">R Shift</div></div>
      <div class="cell"></div>
      <div class="cell"><div class="key">↑</div></div>
      <div class="cell"></div>
      <div class="cell"><div class="key">L Ctrl</div></div>
      <div class="cell"><div class="key">Win</div></div>
      <div class="cell"><div class="key">L Alt</div></div>
      <div class="cell space"><div class="key">Space</div></div>
      <div class="cell"><div class="key">R Alt</div></div>
      <div class="cell"><div class="key">Win</div></div>
      <div class="cell"><div class="key">Fn</div></div>
      <div class="cell"><div class="key">R Ctrl</div></div>
      <div class="cell"><div class="key">←</div></div>
      <div class="cell"><div class="key">↓</div></div>
      <div class="cell"><div class="key">→</div></div>
    </div>
  </div>
</template>

<script>
import { reactive, toRefs, ref, onMounted } from "vue";
import { useStore } from "vuex";
import bus from "@/utils/bus.js";
export default {
  setup() {
    const store = useStore();
    const keybox = ref(null);
    const data = reactive({
      currentKey: "",
    });

    // 测试用 用于点击按钮后模拟按键
    const test = () => {
      downKEY("A");
      setTimeout(() => {
        upKEY("A");
      }, 1000);
    };

    // 用于给pyapi 调用 按下按键
    const downKEY = (keyname) => {
      keybox.value.querySelectorAll(".key").forEach((item) => {
        if (item.innerText == keyname) {
          item.classList.add("active");
          store.state.choose_key = keyname;
          // 修改 那个键位的音效选择框的值
          let mode = store.state.sound_info.mode;
          switch (mode) {
            case "指定":
              console.log(value);
              // 修改配置
              let tmp = "";
              if (store.state.sound_info.assigned_sounds) {
                store.state.sound_info.assigned_sounds.forEach((element) => {
                  if (element.key == store.state.choose_key) {
                    tmp = element;
                  }
                });
              }
              if (tmp != "") {
                store.state.key_sound = tmp.sound;
              }else{
                store.state.key_sound = ""
              }
              break;
            case "单键随机":
              let tmp_ = "";
              if (store.state.sound_info.single_key){
                store.state.sound_info.single_key.forEach((element) => {
                  if (element == store.state.choose_key) {
                    tmp_ = element;
                  }
                });
              }
              if (tmp_ != "") {
                store.state.onekey_flag = true;
              }else{
                store.state.onekey_flag = false;
              }
              break;
            case "3":
              store.state.sound_info.sound3 = keyname;
              break;
            default:
              break;
          }
        }
      });
    };
    // 用于给pyapi 调用 松开按键
    const upKEY = (keyname) => {
      keybox.value.querySelectorAll(".key").forEach((item) => {
        if (item.innerText == keyname) {
          item.classList.remove("active");
          console.log("松qqqq开:", keyname);
        }
      });
    };

    onMounted(() => {
      // 给每个按键绑定事件 按下时调用pyapi的playSound方法
      keybox.value.querySelectorAll(".key").forEach((item) => {
        if (item.innerText != "") {
          item.addEventListener("mousedown", () => {
            pywebview.api.playSound(item.innerText);
            downKEY(item.innerText);
          });
          item.addEventListener("mouseup", () => {
            upKEY(item.innerText);
          });
        }
      });
    });

    // 把downKEY和upKEY挂在window上，方便外部调用
    window.downKEY = downKEY;
    window.upKEY = upKEY;

    return {
      ...toRefs(data),
      test,
      keybox,
    };
  },
};
</script>

<style lang="less" scoped>
.keyboard {
  width: 100%;
  // height: 350px;
  height: 65%;
  background: #fff;
  box-sizing: border-box;
  .keyboard_box {
    width: 100%;
    height: 100%;
    padding: 10px;
    background: #929292;
    display: grid;
    grid-template-columns: repeat(18, 1fr);
    grid-template-rows: repeat(6, 1fr);
  }
}
.cell {
  padding: 4px;
}
.key {
  width: 100%;
  height: 100%;
  color: #fff;
  font-size: 13px;
  display: flex;
  justify-content: center;
  align-items: center;
  border-radius: 10px;
  background: #333;
  transition: background 0.3s;
  cursor: pointer;
  user-select: none;
  position: relative;
  /deep/.tooltip {
    position: absolute;
    top: 0;
    left: 0;
    background: red;
  }
  &:hover {
    // 显示alt
    &::after {
      content: attr(alt);
      position: absolute;
      top: 0;
      left: 0;
      background: #333;
      border-radius: 5px;
    }
  }
}
.key:active {
  background: linear-gradient(315deg, #666, #666);
}
.active {
  background: linear-gradient(315deg, #666, #666);
}
.backspace {
  grid-column: 14 / 16;
}
.tab {
  grid-column: 1 / 3;
}
.caps {
  grid-column: 1 / 3;
}
.enter {
  grid-column: 14 / 16;
}
.shift-left {
  grid-column: 1 / 4;
}
.shift-right {
  grid-column: 14 / 16;
}
.space {
  grid-column: 4 / 12;
}
/deep/.keybind {
  background-color: skyblue !important;

  // font-size: 50px;
}
/deep/.el-button{
  height: 100%;
}

</style>
