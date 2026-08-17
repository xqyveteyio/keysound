# -*- coding:utf-8 _*-
# Linux 托盘图标。
# 没有用 pystray：它在 Linux 上的后端会自己起一个 GTK 主循环，
# 而 pywebview 的 GTK 后端也要在主线程跑主循环，两个循环凑一起必然出问题。
# 所以这里直接用 AppIndicator(StatusNotifierItem)，和 pywebview 共用同一个 GTK 主循环。
import importlib
import os

from PIL import Image

_ICON_NAME = 'keysound-tray'
_ICON_SIZE = 64

# GTK 的对象被回收掉图标就没了，所以要一直拿着引用
_keep_alive = []


# AppIndicator 只认图标主题里的名字，所以把 logo.ico 转成 png 单独放一个目录当主题路径
def _make_icon_theme(icon_path, icon_dir):
    os.makedirs(icon_dir, exist_ok=True)
    png_path = os.path.join(icon_dir, f'{_ICON_NAME}.png')
    with Image.open(icon_path) as image:
        icon = image.convert('RGBA')
        icon.thumbnail((_ICON_SIZE, _ICON_SIZE))
        icon.save(png_path, 'PNG')
    return os.path.abspath(icon_dir)


# AyatanaAppIndicator3 是还在维护的那个分支，没有再退回老的 AppIndicator3
def _import_indicator(gi):
    errors = []
    for name in ('AyatanaAppIndicator3', 'AppIndicator3'):
        try:
            gi.require_version(name, '0.1')
            return importlib.import_module(f'gi.repository.{name}')
        except (ImportError, ValueError) as e:
            errors.append(f'{name}: {e}')
    raise ImportError('没有找到 AppIndicator：' + '；'.join(errors))


# 创建托盘图标，返回 False 表示托盘不可用（调用方据此决定关窗口时是否退出程序）
# 必须在主线程、webview.start() 之前调用，这样图标才和 pywebview 跑在同一个 GTK 主循环里
def start_tray(on_show, on_quit, icon_path='logo.ico', icon_dir='tmp'):
    try:
        import gi
        gi.require_version('Gtk', '3.0')
        from gi.repository import Gtk
        app_indicator = _import_indicator(gi)
        theme_path = _make_icon_theme(icon_path, icon_dir)

        indicator = app_indicator.Indicator.new_with_path(
            'keysound',
            _ICON_NAME,
            app_indicator.IndicatorCategory.APPLICATION_STATUS,
            theme_path,
        )
        indicator.set_title('KeySound')

        menu = Gtk.Menu()
        show_item = Gtk.MenuItem.new_with_label('显示')
        show_item.connect('activate', lambda *_: on_show())
        menu.append(show_item)
        quit_item = Gtk.MenuItem.new_with_label('退出')
        quit_item.connect('activate', lambda *_: on_quit())
        menu.append(quit_item)
        menu.show_all()

        indicator.set_menu(menu)
        # 中键点击图标直接显示窗口
        indicator.set_secondary_activate_target(show_item)
        indicator.set_status(app_indicator.IndicatorStatus.ACTIVE)

        _keep_alive.extend((indicator, menu, show_item, quit_item))
        print('托盘图标已创建（GNOME 需要装 AppIndicator 扩展才能看到托盘）')
        return True
    except Exception as e:
        print('托盘图标不可用，关闭窗口即退出程序:', e)
        return False
