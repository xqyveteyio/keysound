# -*- coding:utf-8 _*-
import os
import sys

# 配置和音效都按当前目录读写，必须在导入那些模块之前切过来。
# 从开始菜单打开时，工作目录不一定是安装目录。
from utils.platform_util import acquire_single_instance, app_dir, show_warning
os.chdir(app_dir())

import webview
import threading
from utils.mouse_util import *
from utils.keyboard_util import *
from utils.sound_util import *
from utils.window_util import *
from utils import vmic_win


if __name__ == '__main__':
  if not os.path.exists("tmp"):
    os.mkdir("tmp")
  me = acquire_single_instance() # 返回 None 说明已经有一个实例在跑了
  if me is None:
    print("已经有一个实例在运行了!")
    show_warning("提示", "KeySound 已经在运行了\n请留意右下角图标")
    sys.exit(-1)

  # 虚拟麦克风的混音线程：开关是关的时候它只是空转，开着才会去连共享内存
  vmic_win.start()

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
