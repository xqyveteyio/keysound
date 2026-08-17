import pynput
from config.global_config import global_config_obj
from config.window_config import window_config_obj
import threading

def on_scroll(x, y, dx, dy):
    print('Scrolled {0} at {1}'.format(
        'down' if dy < 0 else 'up',
        (x, y)))

# 鼠标监听回调
def on_move(x, y):
    print('Pointer moved to {0}'.format((x, y)))
    if window_config_obj.stop_mouse:
      return False

  # 启动鼠标监听
def start_mouse():
    with pynput.mouse.Listener(
        on_click=on_click,
    ) as listener:
        listener.join()


def on_click(x, y, button, pressed):
      # print(f"{'点击' if pressed else '抬起'}了{button}")
      if global_config_obj.mouse_flag:
        if pressed:
          print('点击了', button)
          if button == pynput.mouse.Button.left:
            # self.window.evaluate_js(f'window.mouse_down("l")')
            threading.Thread(target=window_config_obj.window.evaluate_js, args=(f'window.mouse_down("l")',)).start()
          elif button == pynput.mouse.Button.right:
            # self.window.evaluate_js(f'window.mouse_down("r")')
            threading.Thread(target=window_config_obj.window.evaluate_js, args=(f'window.mouse_down("r")',)).start()
        else:
          print('抬起了', button)
          if button == pynput.mouse.Button.left:
            # self.window.evaluate_js(f'window.mouse_up("l")')
            threading.Thread(target=window_config_obj.window.evaluate_js, args=(f'window.mouse_up("l")',)).start()
          elif button == pynput.mouse.Button.right:
            # self.window.evaluate_js(f'window.mouse_up("r")')
            threading.Thread(target=window_config_obj.window.evaluate_js, args=(f'window.mouse_up("r")',)).start()
