from utils.keyfilter import keyfilter
import threading
from config.window_config import window_config_obj
from utils.sound_util import playSound, stopSound
import multiprocessing
from config.global_config import all_sounds, global_config_obj
import keyboard

keyList = {}

# 设置页正在录快捷键时，按键既不出声也不触发快捷键
_recording_hotkey = False

# 快捷键里的修饰键不分左右，两个 Ctrl 都算 Ctrl
_MODIFIER_ALIAS = {
    'L Ctrl': 'Ctrl',
    'R Ctrl': 'Ctrl',
    'L Alt': 'Alt',
    'R Alt': 'Alt',
    'L Shift': 'Shift',
    'R Shift': 'Shift',
}
_MODIFIERS = frozenset(('ctrl', 'alt', 'shift', 'win'))


# 给设置页用：录快捷键期间把按键处理停掉
def set_recording_hotkey(flag):
    global _recording_hotkey
    _recording_hotkey = bool(flag)
    return _recording_hotkey


def _norm_key(key):
    return _MODIFIER_ALIAS.get(key, key).lower()


# 刚按下的这个键是不是配置里的停止快捷键。
# 快捷键存成 "Ctrl+Alt+S"：最后一段是主键，前面几段是要同时按住的修饰键
def _is_stop_hotkey(key):
    hotkey = (global_config_obj.__dict__.get('stop_hotkey') or '').strip()
    parts = [p for p in (s.strip() for s in hotkey.split('+')) if p]
    if not parts:
        return False
    if _norm_key(key) != _norm_key(parts[-1]):
        return False
    # 修饰键要不多不少：配了 Ctrl+Alt+S 的话，多按着一个 Shift 就是另一个组合了
    held = {_norm_key(k) for k, is_down in keyList.items() if is_down}
    return {_norm_key(m) for m in parts[:-1]} == held & _MODIFIERS

# 启动键盘监听线程
def on_key_event():
    keyboard.hook(callback)
    keyboard.wait()


# 监听键盘事件
def callback(event):
    event.name = event.name[0].upper() + event.name[1:]
    event.name = keyfilter(event.name)
    handle_key(event.name, event.event_type == 'down', update_ui=True)


# 按键事件的公共处理：出声 + 刷新键盘 UI
def handle_key(key, is_down, update_ui):
    if _recording_hotkey:
        return
    if key not in keyList:
        keyList[key] = False
    if is_down and keyList[key] == False:
        print(key, ' is down')
        keyList[key] = True
        if _is_stop_hotkey(key):
            stopSound()
            # 快捷键本身不再触发音效，不然刚停就又响一个
            if update_ui:
                threading.Thread(target=downKey, args=(key,)).start()
            return
        if global_config_obj.single_flag:
            global_config_obj.break_flag = False
        if global_config_obj.now_play and global_config_obj.single_flag:
            return
        global_config_obj.now_play = True
        if update_ui:
            threading.Thread(target=downKey, args=(key,)).start()
        print(all_sounds)
        print("global_config_obj.break_flag:", global_config_obj.break_flag)
        if global_config_obj.break_flag:
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
