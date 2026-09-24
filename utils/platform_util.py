# -*- coding:utf-8 _*-
# 只支持 Windows。窗口用系统自带的 Edge WebView2，不带 Qt。
import sys

if sys.platform != 'win32':
    raise SystemExit('KeySound 只支持 Windows')

# pywebview 的 GUI 后端：edgechromium 就是 Edge WebView2
WEBVIEW_GUI = 'edgechromium'

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
