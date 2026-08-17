from utils.keyfilter import keyfilter
import threading
from config.window_config import window_config_obj
from utils.sound_util import playSound
import multiprocessing
from config.global_config import all_sounds, global_config_obj
from utils.platform_util import IS_WINDOWS

if IS_WINDOWS:
    import keyboard

keyList = {}

# 启动键盘监听线程
def on_key_event():
    if not IS_WINDOWS:
        # Linux 上 keyboard 库要 root 才能读 /dev/input，Wayland 也没有全局钩子，
        # 所以只监听窗口内的按键（见 utils/web_key.py），这里直接退出线程
        print('非 Windows 平台不做全局键盘监听，只在 KeySound 窗口聚焦时响应按键')
        return
    keyboard.hook(callback)
    keyboard.wait()


# 监听键盘事件
def callback(event):
    event.name = event.name[0].upper() + event.name[1:]
    event.name = keyfilter(event.name)
    handle_key(event.name, event.event_type == 'down', update_ui=True)


# 网页里的按键事件（Linux 下由注入的 JS 回传，页面自己做高亮所以不用再回写 UI）
def web_key_event(key, is_down):
    handle_key(key, bool(is_down), update_ui=False)
    return True


# 按键事件的公共处理：出声 + 刷新键盘 UI
def handle_key(key, is_down, update_ui):
    if key not in keyList:
        keyList[key] = False
    if is_down and keyList[key] == False:
        print(key, ' is down')
        keyList[key] = True
        if global_config_obj.single_flag:
            global_config_obj.break_flag = False
        if global_config_obj.now_play and global_config_obj.single_flag:
            return
        global_config_obj.now_play = True
        if update_ui:
            threading.Thread(target=downKey, args=(key,)).start()
        print(all_sounds)
        print("global_config_obj.break_flag:", global_config_obj.break_flag)
        if global_config_obj.break_flag and IS_WINDOWS:
            print("进入break")
        # threading.Thread(target=playSound, args=(key,)).start()
            p = multiprocessing.Process(target=playSound, args=(key,))
            print(all_sounds)
            if len(all_sounds) >= 3:
                print("打断")
                all_sounds[0][1].terminate()
                del all_sounds[0]
            all_sounds.append((p.name, p))
            p.start()
        else:
            # Linux 的打断由播放后端自己掐掉多余的声音，不用起多进程
            threading.Thread(target=playSound, args=(key,)).start()

    elif not is_down and keyList[key] == True:
        print(key, ' is up')
        keyList[key] = False
        if update_ui:
            threading.Thread(target=upKey, args=(key,)).start()

# 按键按下
def downKey(key):
    if window_config_obj.window_flag:
        print('down---------------------')
        window_config_obj.window.evaluate_js(f'window.downKEY("{key}")')

# 按键抬起
def upKey(key):
    if window_config_obj.window_flag:
        print('up---------------------')
        window_config_obj.window.evaluate_js(f'window.upKEY("{key}")')
