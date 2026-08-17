# -*- coding:utf-8 _*-
import webview
import threading
import os
import win32api
import win32con
import sys
from tendo import singleton
from utils.mouse_util import *
from utils.keyboard_util import *
from utils.sound_util import *
from utils.window_util import *


if __name__ == '__main__':
  if not os.path.exists("tmp"):
    os.mkdir("tmp")
  try:
    me = singleton.SingleInstance() # will sys.exit(-1) if other instance is running
  except:
    print("已经有一个实例在运行了!")
    win32api.MessageBox(0, "KeySound 已经在运行了\n请留意右下角图标", "提示",win32con.MB_ICONWARNING)
    sys.exit(-1)

  event1 = threading.Event()
  # 开线程监听键盘
  t1 = threading.Thread(target=on_key_event)
  t1.daemon = True
  t1.start()
  # 开线程创建菜单
  t2 = threading.Thread(target=create_menu, args=(event1,))
  t2.daemon = True
  t2.start()
  while True:
    event1.wait()
    event1.clear()
    create_window_()




