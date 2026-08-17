from config.window_config import window_config_obj
import os
from PIL import Image
from config.global_config import global_config_obj
import webview
from utils.mouse_util import *
from utils.keyboard_util import *
from utils.sound_util import *
import json
from utils.api import *
from utils.platform_util import IS_WINDOWS, WEBVIEW_GUI, set_linux_autostart
from utils.web_key import inject_web_key_listener

if IS_WINDOWS:
    import winreg
    import pystray
    from pystray import MenuItem
    from PyQt5 import QtGui
elif WEBVIEW_GUI == 'gtk':
    # 托盘要和 webview 共用同一个事件循环，所以跟着后端选实现
    from utils.tray_linux import start_tray
else:
    from utils.tray_qt import start_tray

# 托盘是否真的建起来了，没有托盘的话关掉窗口就该退出程序
tray_running = False


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
    if not IS_WINDOWS and tray_running:
      # 非 Windows 上 webview.start() 不能重复调用，窗口销毁了就再也开不起来，
      # 所以只把窗口藏起来，返回 False 取消这次关闭
      window_config_obj.window.hide()
      return False

# 发信号给主线程创建window （webview必须在主线程创建）
def test():
    if not IS_WINDOWS and window_config_obj.window is not None:
      # Linux 上窗口只是被藏起来了，直接显示回来
      window_config_obj.window_flag = True
      window_config_obj.window.show()
      return
    if not window_config_obj.window_flag:
      window_config_obj.event.set()
    window_config_obj.window_flag = True


# 托盘菜单：退出程序
def quit_app():
    os._exit(0)


# 开机自启
def auto_start(flag):
    global_config_obj.auto_run = flag
    # 写入本地config.json
    global_config_obj.save()
    if not IS_WINDOWS:
        # Linux 没有注册表，用 XDG autostart 的 .desktop 文件代替
        set_linux_autostart(global_config_obj.auto_run)
        return
    if global_config_obj.auto_run:
      # 添加开机自启注册表
      key = winreg.OpenKey(winreg.HKEY_CURRENT_USER, r'Software\Microsoft\Windows\CurrentVersion\Run', 0, winreg.KEY_ALL_ACCESS)
      winreg.SetValueEx(key, 'KeySound', 0, winreg.REG_SZ, rf"{os.getcwd()}\KeySound.exe")
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

# 注入主题：主题就是 themes/<主题名>.css，里面只需要重新赋值 :root 上那套颜色变量。
# "默认"（浅色）没有对应文件，也就等于不注入
def inject_theme():
  path = os.path.join('themes', f'{global_config_obj.theme}.css')
  if not os.path.isfile(path):
    return
  with open(path, encoding='utf-8') as f:
    css = f.read()
  if css.strip():
    window_config_obj.window.load_css(css)


# 创建window
def create_window_():
    global tray_running
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
    if not IS_WINDOWS:
      # Linux 下键盘事件由页面自己上报，页面每次加载完都要重新注入监听
      window_config_obj.window.expose(web_key_event)
      window_config_obj.window.events.loaded += on_loaded
      # 托盘要和 pywebview 共用同一个事件循环，所以在主线程里、start() 之前创建
      tray_running = start_tray(test, quit_app)
    # 查看本地debug.txt是否存在 存在则开启debug模式
    tmp = False
    if os.path.exists('debug.txt'):
      tmp = True
    else:
      tmp = False
    webview.start(debug=tmp, gui=WEBVIEW_GUI)
    if not tray_running:
      # 没有托盘时窗口关掉就再也打不开了，直接退出，别留一个看不见的进程
      os._exit(0)


# 页面加载完成
def on_loaded(*_args):
    inject_web_key_listener(window_config_obj.window)


# 创建菜单
def create_menu(event):
    global tray_running
    window_config_obj.event = event
    if not IS_WINDOWS:
      # Linux 的托盘在 create_window_ 里跟着 GTK 主循环一起建，这里只负责把窗口拉起来
      test()
      return
    tray_running = True
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
