# -*- coding:utf-8 _*-
# 平台差异都收敛在这个文件里：Windows 分支保持原来的行为，
# Linux 分支用等价实现替掉 win32api / winreg 之类只有 Windows 才有的接口。
import os
import shutil
import subprocess
import sys
import tempfile

IS_WINDOWS = sys.platform == 'win32'
IS_LINUX = sys.platform.startswith('linux')
IS_MACOS = sys.platform == 'darwin'

# webview 的 GUI 后端。
# Windows 上留空让 pywebview 自己挑，也就是 EdgeChromium（WebView2）：系统自带、
# 内核跟得上，不用为了一个窗口把整套 PyQt5 + QtWebEngine 拖进来。
# Linux 上没有 WebView2，固定用 qt（QtWebEngine）：窗口装饰和页面渲染都跟 Chromium 一致，
# 而且不会碰到 gtk 后端那两个坑（Wayland 显式同步崩溃、evaluate_js 把脚本长度按字符数
# 传给 WebKit 导致中文脚本被截断）。想试 GTK + WebKit2 就把这里改成 'gtk'。
WEBVIEW_GUI = None if IS_WINDOWS else 'qt'

# Wayland 下 WebKit 的 dmabuf 渲染器会给窗口的 wl_surface 挂上显式同步（wp_linux_drm_syncobj），
# 而 GTK3 自己画窗口边框用的是 shm 缓冲，合成器认为违反协议就断开连接，
# 表现是启动没几秒就 "Error 71 (Protocol error) dispatching to Wayland display" 整个进程退出。
# 关掉 dmabuf 渲染器（改走 shm）可以绕开，代价是渲染慢一点。
# 已经手动设过这个变量就不覆盖，方便自己试别的取值。
# 不只在 WEBVIEW_GUI == 'gtk' 时设：Qt 装不全的时候 pywebview 会自己回退到 gtk，
# 这个变量对 Qt 后端没有任何影响，索性一直设上。
if (IS_LINUX and os.environ.get('WAYLAND_DISPLAY')
        and 'WEBKIT_DISABLE_DMABUF_RENDERER' not in os.environ):
    os.environ['WEBKIT_DISABLE_DMABUF_RENDERER'] = '1'

# 单实例锁的句柄要一直被引用着，否则被回收后锁就没了
_instance_lock = None


# 获取单实例锁，返回 None 表示已经有一个实例在跑了
def acquire_single_instance():
    global _instance_lock
    if IS_WINDOWS:
        from tendo import singleton
        try:
            _instance_lock = singleton.SingleInstance()
        except BaseException:
            return None
        return _instance_lock

    import fcntl
    lock_path = os.path.join(tempfile.gettempdir(), f'keysound-{os.getuid()}.lock')
    lock_file = open(lock_path, 'w')
    try:
        fcntl.flock(lock_file, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except OSError:
        lock_file.close()
        return None
    lock_file.write(str(os.getpid()))
    lock_file.flush()
    _instance_lock = lock_file
    return _instance_lock


# 弹提示框（Linux 上没有 win32 的 MessageBox，退化成桌面通知 + 终端输出）
def show_warning(title, message):
    if IS_WINDOWS:
        import win32api
        import win32con
        win32api.MessageBox(0, message, title, win32con.MB_ICONWARNING)
        return

    print(f'[{title}] {message}')
    notify_send = shutil.which('notify-send')
    if notify_send:
        try:
            subprocess.Popen([notify_send, title, message])
        except OSError as e:
            print('发送桌面通知失败:', e)


# Linux 下的开机自启：写 XDG autostart 的 .desktop 文件
def set_linux_autostart(enabled):
    autostart_dir = os.path.expanduser('~/.config/autostart')
    desktop_file = os.path.join(autostart_dir, 'keysound.desktop')
    if not enabled:
        if os.path.exists(desktop_file):
            os.remove(desktop_file)
        return

    work_dir = os.getcwd()
    entry = '\n'.join([
        '[Desktop Entry]',
        'Type=Application',
        'Name=KeySound',
        f'Exec={sys.executable} {os.path.join(work_dir, "main.py")}',
        f'Path={work_dir}',
        f'Icon={os.path.join(work_dir, "logo.ico")}',
        'Terminal=false',
        'X-GNOME-Autostart-enabled=true',
        '',
    ])
    os.makedirs(autostart_dir, exist_ok=True)
    with open(desktop_file, 'w', encoding='utf-8') as f:
        f.write(entry)
