# -*- coding:utf-8 _*-
# 只支持 Windows。窗口用系统自带的 Edge WebView2，不带 Qt。
import os
import sys

if sys.platform != 'win32':
    raise SystemExit('KeySound 只支持 Windows')

# pywebview 的 GUI 后端：edgechromium 就是 Edge WebView2
WEBVIEW_GUI = 'edgechromium'


# 程序自己的目录。打包之后 PyInstaller 的 _MEIPASS 指向 _internal，
# 但 ui、音效、配置都放在 exe 旁边，不能去 _internal 里找。
def app_dir():
    if getattr(sys, 'frozen', False):
        return os.path.dirname(os.path.abspath(sys.executable))
    return os.path.abspath('.')

# 单实例锁的句柄要一直被引用着，否则被回收后锁就没了
_instance_lock = None


# 获取单实例锁，返回 None 表示已经有一个实例在跑了
def acquire_single_instance():
    global _instance_lock
    from tendo import singleton
    try:
        _instance_lock = singleton.SingleInstance()
    except BaseException:
        return None
    return _instance_lock


# 弹提示框
def show_warning(title, message):
    import win32api
    import win32con
    win32api.MessageBox(0, message, title, win32con.MB_ICONWARNING)
