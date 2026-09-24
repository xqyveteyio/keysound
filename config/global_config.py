import os
import json
from config.window_config import window_config_obj
class GlobalConfig:
    def __init__(self) -> None:
        self.keyboard_flag = True
        self.mouse_flag = False
        self.break_flag = False
        self.single_flag = False
        self.now_play = False
        self.choose_sound = "老婆的声音"
        self.auto_run = False
        self.font = "微软雅黑"
        self.theme = "默认"
        # 停止播放的快捷键，形如 "Ctrl+Alt+S"，空字符串表示没设
        self.stop_hotkey = ""
        # 虚拟麦克风输出：把音效混进选定的麦克风信号里，让语音软件里的其他人也能听到
        self.virtual_mic = False
        # 挂在哪只麦克风上，存 MMDevices 里的端点 GUID，空字符串表示还没选
        self.virtual_mic_device = ""
        # APO 挂在哪个槽位：efx / mfx / sfx，不同机器能用的不一样
        self.virtual_mic_slot = "sfx"
        # 混进麦克风那一路的音量，100 表示和音箱那一路一样大
        self.virtual_mic_volume = 100
        # 查看是否有config.json
        if not os.path.exists(f'config.json'):
            # 没有就创建
            with open('config.json', 'w', encoding='utf-8') as f:
                json.dump(self.__dict__, f, ensure_ascii=False, indent=2)
                print('创建config.json')
        # 查看是否有sounds文件夹
        if not os.path.exists('sounds'):
            # 没有就创建
            os.mkdir('sounds')
            print('创建sounds文件夹')
        self.reload()

    def reload(self):
        with open('config.json', 'r', encoding='utf-8') as f:
            self.__dict__.update(json.loads(f.read()))
        # 旧版本的主题名："默认" 那时候是深色，另外还有一个叫 "白" 的浅色主题。
        # 现在默认就是浅色，深色变成可选项，老配置照着新名字换一下
        self.theme = {'白': '默认', '黑': '深色'}.get(self.theme, self.theme)

    def save(self):
        with open('config.json', 'w', encoding='utf-8') as f:
            json.dump(self.__dict__, f, ensure_ascii=False, indent=4)

global_config_obj = GlobalConfig()
global_config_obj.now_play = False
global_config_obj.save()
all_sounds = []