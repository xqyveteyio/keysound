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

# webview 的 GUI 后端：Windows 沿用 qt，Linux 用 gtk（PyGObject + WebKit2）
WEBVIEW_GUI = 'qt' if IS_WINDOWS else 'gtk'

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
        f'Exec={sys.executable} {os.path.join(work_dir, "KeySound.py")}',
        f'Path={work_dir}',
        f'Icon={os.path.join(work_dir, "logo.ico")}',
        'Terminal=false',
        'X-GNOME-Autostart-enabled=true',
        '',
    ])
    os.makedirs(autostart_dir, exist_ok=True)
    with open(desktop_file, 'w', encoding='utf-8') as f:
        f.write(entry)
