# -*- mode: python ; coding: utf-8 -*-
# 打成 onedir：程序用的是当前工作目录下的 ui/、themes/、config.json，
# 单文件版会解压到临时目录，这些相对路径就对不上了。
# ui、themes、两个原生文件不塞进包里，编完由工作流拷到 exe 旁边，
# 因为代码读的是工作目录，不是 PyInstaller 的 _internal。

import os

from PyInstaller.utils.hooks import collect_all, collect_submodules

root = os.path.abspath(os.path.join(SPECPATH, '..'))

datas = []
binaries = []
hiddenimports = []
# pywebview 走 pythonnet 调 WebView2，漏收集的话窗口直接起不来
for package in ('webview', 'pythonnet', 'clr_loader'):
    package_datas, package_binaries, package_hidden = collect_all(package)
    datas += package_datas
    binaries += package_binaries
    hiddenimports += package_hidden

hiddenimports += collect_submodules('pystray')
hiddenimports += collect_submodules('pynput')
hiddenimports += [
    'win32api',
    'win32con',
    'tendo.singleton',
    'playsound',
    'keyboard',
    'miniaudio',
    'PIL.Image',
]

a = Analysis(
    [os.path.join(root, 'main.py')],
    pathex=[root],
    binaries=binaries,
    datas=datas,
    hiddenimports=hiddenimports,
    # 窗口走系统 WebView2。本机如果装了 PyQt，collect webview 会把它整份打进来
    excludes=['PyQt5', 'PyQt6', 'PySide2', 'PySide6', 'qtpy'],
    noarchive=False,
)
pyz = PYZ(a.pure)

exe = EXE(
    pyz,
    a.scripts,
    [],
    exclude_binaries=True,
    name='KeySound',
    icon=os.path.join(root, 'logo.ico'),
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=False,
    console=False,
    disable_windowed_traceback=False,
)

coll = COLLECT(
    exe,
    a.binaries,
    a.datas,
    strip=False,
    upx=False,
    name='KeySound',
)
