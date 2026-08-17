import keyboard
from utils.keyfilter import keyfilter
import threading
from config.window_config import window_config_obj
from utils.sound_util import playSound
import multiprocessing
from config.global_config import all_sounds, global_config_obj

keyList = {}

# 启动键盘监听线程
def on_key_event():
    keyboard.hook(callback)
    keyboard.wait()


# 监听键盘事件
def callback(event):
    event.name = event.name[0].upper() + event.name[1:]
    event.name = keyfilter(event.name)
    if event.name not in keyList:
        keyList[event.name] = False
    if event.event_type == 'down' and keyList[event.name] == False:
        print(event.name,' is down')
        keyList[event.name] = True
        if global_config_obj.single_flag:
            global_config_obj.break_flag = False
        if global_config_obj.now_play and global_config_obj.single_flag:
            return
        global_config_obj.now_play = True
        threading.Thread(target=downKey, args=(event.name,)).start()
        print(all_sounds)
        print("global_config_obj.break_flag:", global_config_obj.break_flag)
        if global_config_obj.break_flag:
            print("进入break")
        # threading.Thread(target=playSound, args=(event.name,)).start()
            p = multiprocessing.Process(target=playSound, args=(event.name,))
            print(all_sounds)
            if len(all_sounds) >= 3:
                print("打断")
                all_sounds[0][1].terminate()
                del all_sounds[0]
            all_sounds.append((p.name, p))
            p.start()
        else:
            threading.Thread(target=playSound, args=(event.name,)).start()
        
    elif event.event_type == 'up' and keyList[event.name] == True:
        print(event.name, ' is up')
        keyList[event.name] = False
        threading.Thread(target=upKey, args=(event.name,)).start()

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
