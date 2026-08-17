# -*- coding:utf-8 _*-
# Qt 托盘图标，配合 pywebview 的 qt 后端（QtWebEngine）使用。
# 和 GTK 那套一样走 StatusNotifierItem，所以 GNOME 同样要装 AppIndicator 扩展才看得到。
# pywebview 的 qt 后端建 app 时用的是 QApplication.instance() or QApplication(sys.argv)，
# 所以这里先把 QApplication 建好，托盘就和窗口共用同一个 Qt 事件循环。
import os
import sys

# Qt 对象没人引用就会被回收，图标也跟着消失，所以要一直拿着
_keep_alive = []


# 创建托盘图标，返回 False 表示托盘不可用（调用方据此决定关窗口时是否退出程序）
# 必须在主线程、webview.start() 之前调用
def start_tray(on_show, on_quit, icon_path='logo.ico'):
    try:
        from qtpy.QtGui import QIcon
        from qtpy.QtWidgets import QApplication, QMenu, QSystemTrayIcon

        # QtWebEngine 要求在 QApplication 之前导入（否则得自己设 AA_ShareOpenGLContexts），
        # 这里比 pywebview 先建 app，所以这个导入得由我们来做
        import qtpy.QtWebEngineWidgets  # noqa: F401

        app = QApplication.instance() or QApplication(sys.argv)
        # 不设的话托盘项的名字会取 argv[0]（也就是 main.py），鼠标悬停和菜单标题都不好看
        app.setApplicationName('KeySound')
        app.setApplicationDisplayName('KeySound')
        app.setDesktopFileName('keysound')
        if not QSystemTrayIcon.isSystemTrayAvailable():
            raise RuntimeError('系统托盘不可用')

        icon = QIcon(os.path.abspath(icon_path))
        if icon.isNull():
            raise RuntimeError(f'图标读不出来: {icon_path}')

        menu = QMenu()
        show_action = menu.addAction('显示')
        show_action.triggered.connect(lambda *_: on_show())
        quit_action = menu.addAction('退出')
        quit_action.triggered.connect(lambda *_: on_quit())

        tray = QSystemTrayIcon(icon)
        tray.setToolTip('KeySound')
        tray.setContextMenu(menu)

        # 点图标显示窗口。具体哪个键触发由桌面环境决定，
        # GNOME 的 AppIndicator 扩展左键只弹菜单，中键才会走到这里
        click_reasons = (
            QSystemTrayIcon.ActivationReason.Trigger,
            QSystemTrayIcon.ActivationReason.MiddleClick,
            QSystemTrayIcon.ActivationReason.DoubleClick,
        )
        tray.activated.connect(lambda reason: on_show() if reason in click_reasons else None)
        tray.show()

        # 窗口藏起来时 Qt 默认会认为没窗口了就退出事件循环，托盘还在就不能让它退
        app.setQuitOnLastWindowClosed(False)

        _keep_alive.extend((app, tray, menu, show_action, quit_action))
        print('托盘图标已创建（GNOME 需要装 AppIndicator 扩展才能看到托盘）')
        return True
    except Exception as e:
        print('托盘图标不可用，关闭窗口即退出程序:', e)
        return False
