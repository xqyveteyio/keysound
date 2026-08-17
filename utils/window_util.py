from config.window_config import window_config_obj
import os
from pystray import MenuItem
import pystray
from PIL import Image
from config.global_config import global_config_obj
import winreg
import webview
from utils.mouse_util import *
from utils.keyboard_util import *
from utils.sound_util import *
import json
from utils.api import *
from PyQt5 import QtGui



# 初始化函数
def initUI():
  global_config_obj.reload()
  return global_config_obj.choose_sound

# 销毁window
def destroy_window():
    window_config_obj.window.destroy()
    window_config_obj.window_flag = False

# 窗口即将关闭事件
def on_closing():
    print('窗口即将关闭')
    window_config_obj.window_flag = False

# 发信号给主线程创建window （webview必须在主线程创建）
def test():
    if not window_config_obj.window_flag:
      window_config_obj.event.set()
    window_config_obj.window_flag = True


# 开机自启
def auto_start(flag):
    global_config_obj.auto_run = flag
    # 写入本地config.json
    global_config_obj.save()
    if global_config_obj.auto_run:
      # 添加开机自启注册表
      key = winreg.OpenKey(winreg.HKEY_CURRENT_USER, r'Software\Microsoft\Windows\CurrentVersion\Run', 0, winreg.KEY_ALL_ACCESS)
      winreg.SetValueEx(key, 'KeySound', 0, winreg.REG_SZ, f"{os.getcwd()}\KeySound.exe")
      winreg.CloseKey(key)
    else:
      # 删除开机自启注册表
      key = winreg.OpenKey(winreg.HKEY_CURRENT_USER, r'Software\Microsoft\Windows\CurrentVersion\Run', 0, winreg.KEY_ALL_ACCESS)
      winreg.DeleteValue(key, 'KeySound')
      winreg.CloseKey(key)


# 键盘 鼠标 总开关
def globalSwitch(open1, switch1, switch2):
    if open1:
      global_config_obj.keyboard_flag = switch1
      global_config_obj.mouse_flag = switch2
      global_config_obj.save()
    else:
      # 读取本地config.json
      global_config_obj.reload()
      return global_config_obj.keyboard_flag,global_config_obj.mouse_flag

# 注入主题（加载css）
def inject_theme():
  if global_config_obj.theme == "白":
    window_config_obj.window.load_css("""
   * {
  color: #3f3f3f !important;
  background: #fff !important;
}

.key {
  background: #ececec !important;
  color: #3f3f3f !important;
}

.key:hover::after {
  content: attr(alt);
  position: absolute;
  top: 0;
  left: 0;
  background: #ffffffbe !important;
  border-radius: 5px;
}

.active {
  background: linear-gradient(315deg, #cacaca, #cacaca) !important;
}

.el-switch__core .el-switch__action {
  background: #cacaca !important;
}

.el-switch.is-checked .el-switch__core .el-switch__action {
  background: skyblue !important;
}

.item:hover span {
  background: #ececec !important;
  transition: all 0.2s;
}

.item:hover span:last-child {
  color: red !important;
}

.menu {
  border-right: 1px solid #ebeef5 !important;
}

.router-link-exact-active i {
  color: #409eff !important;
}

.el-select-dropdown__item:hover {
  background: #ececec !important;
}

.el-select-dropdown__item:hover span {
  background: #ececec !important;
}

.el-checkbox__input.is-checked .el-checkbox__inner::after {
  background: skyblue !important;
}
   
   """)


# 创建window
def create_window_():
    window_config_obj.window = webview.create_window('KeySound', './ui/index.html', width=1200, height=560,text_select=False,resizable=True)
    # window_config_obj.window = webview.create_window('KeySound', 'http://127.0.0.1:8080/', width=1200, height=560,text_select=False,resizable=True)
    window_config_obj.window.events.closing += on_closing
    window_config_obj.window.expose(getSoundList,
                       getSoundInfo,
                       updateSoundInfo,
                       newSoundInfo,
                       delSoundInfo,
                       addSoundFile,
                       selectSound,
                       initUI,
                       playSound,
                       exportSound,
                       importSound,
                       single_key_switch,
                       globalSwitch,
                      delSoundFile,
                      auto_start,
                      get_all_switch_state,
                      update_all_switch_state,
                      inject_theme,
                      list_plugins,
                      get_plugin,
                      delete_plugin,
                      upload_plugin,
                      upload_file,
                      download_file,
                      upload_sound
    )
    # 查看本地debug.txt是否存在 存在则开启debug模式
    tmp = False
    if os.path.exists('debug.txt'):
      tmp = True
    else:
      tmp = False
    webview.start(debug=tmp, gui='qt')


# 创建菜单
def create_menu(event):
    window_config_obj.event = event
    test()
    menu = (
      MenuItem(text='显示', action=test, default=True, visible=False),
      MenuItem(text='退出', action=on_exit),
    )
    
    image = Image.open("logo.ico")
    icon = pystray.Icon("name", image, "keysound", menu)
    icon.run()

# 卸载托盘图标
def on_exit(icon):
    icon.visible = False
    icon.stop()
    if window_config_obj.window_flag:
      window_config_obj.window.destroy()
    # 强制退出
    os._exit(0)
