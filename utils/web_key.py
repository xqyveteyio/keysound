# -*- coding:utf-8 _*-
# Linux（尤其 Wayland）下拿不到全局键盘钩子，退化成只监听 KeySound 窗口自己的按键。
# 做法是往页面里注入一段脚本，把 keydown / keyup 翻译成键盘布局上的键名，
# 一份留给页面自己做高亮，一份回传给 Python 播放音效。

_KEY_LISTENER_JS = r"""
(function () {
  if (window.__keysound_window_hook__) {
    return;
  }
  window.__keysound_window_hook__ = true;

  // 页面上键帽的文字（见 web/src/components/KeyBoardView/KeyBoard.vue）
  var CODE_MAP = {
    Escape: "Esc",
    ScrollLock: "SrcLk",
    Pause: "Pause",
    Backquote: "~",
    Minus: "-",
    Equal: "=",
    Backspace: "Backspace",
    Insert: "Ins",
    Home: "Home",
    PageUp: "PgUp",
    Tab: "Tab",
    BracketLeft: "[ {",
    BracketRight: "] }",
    Backslash: "\\",
    Delete: "Del",
    End: "End",
    PageDown: "PgDn",
    CapsLock: "Caps",
    Semicolon: "; :",
    Quote: "'",
    Enter: "Enter",
    NumpadEnter: "Enter",
    ShiftLeft: "L Shift",
    ShiftRight: "R Shift",
    Comma: ", <",
    Period: ". >",
    Slash: "/ ?",
    ControlLeft: "L Ctrl",
    ControlRight: "R Ctrl",
    AltLeft: "L Alt",
    AltRight: "R Alt",
    MetaLeft: "Win",
    MetaRight: "Win",
    OSLeft: "Win",
    OSRight: "Win",
    Space: "Space",
    ArrowUp: "\u2191",
    ArrowDown: "\u2193",
    ArrowLeft: "\u2190",
    ArrowRight: "\u2192"
  };

  var pressed = {};

  function keyName(event) {
    var code = event.code || "";
    if (CODE_MAP[code]) {
      return CODE_MAP[code];
    }
    if (/^Key[A-Z]$/.test(code)) {
      return code.slice(3);
    }
    if (/^Digit[0-9]$/.test(code)) {
      return code.slice(5);
    }
    if (/^F([1-9]|1[0-2])$/.test(code)) {
      return code;
    }
    // 没画在键盘上的键（小键盘之类）就用原始 code，随机/重复模式照样能出声
    return code || event.key;
  }

  function notify(name, isDown) {
    var highlight = isDown ? window.downKEY : window.upKEY;
    if (typeof highlight === "function") {
      try {
        highlight(name);
      } catch (e) {
        // 当前路由不是键盘页面时高亮会失败，忽略
      }
    }
    if (window.pywebview && window.pywebview.api && window.pywebview.api.web_key_event) {
      window.pywebview.api.web_key_event(name, isDown);
    }
  }

  window.addEventListener("keydown", function (event) {
    if (event.repeat) {
      return;
    }
    var name = keyName(event);
    if (pressed[name]) {
      return;
    }
    pressed[name] = true;
    notify(name, true);
  }, true);

  window.addEventListener("keyup", function (event) {
    var name = keyName(event);
    if (!pressed[name]) {
      return;
    }
    delete pressed[name];
    notify(name, false);
  }, true);

  // 窗口失焦时可能收不到 keyup，手动把按下状态清掉，避免键一直卡在按下
  window.addEventListener("blur", function () {
    Object.keys(pressed).forEach(function (name) {
      delete pressed[name];
      notify(name, false);
    });
  });
})();
"""


# 往窗口里注入按键监听（页面每次加载完都要重新注入）
def inject_web_key_listener(window):
    try:
        window.evaluate_js(_KEY_LISTENER_JS)
        print('已注入窗口按键监听（Linux 只能在 KeySound 窗口聚焦时响应按键）')
    except Exception as e:
        print('注入窗口按键监听失败:', e)
