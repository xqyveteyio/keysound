from config.global_config import global_config_obj
from config.sound_config import SoundConfig
import playsound
import random
import os
import json
import shutil
import webview
from config.window_config import window_config_obj
import multiprocessing
from config.global_config import all_sounds, global_config_obj
from utils.api import upload_file, upload_plugin


# 导出音效
def exportSound(name):
    print("导出音效---------------------")
    file_types = ('Image Files (*.zip)', 'All files (*.*)')
    print(name)
    result = window_config_obj.window.create_file_dialog(webview.SAVE_DIALOG, allow_multiple=False, file_types=file_types,save_filename=name)
    if result:
      print("导出:", result)
      target = result[0]
      shutil.make_archive(target, 'zip', f'./sounds/{name}')
      return True
    return False

# 导入音效
def importSound():
    file_types = ('Image Files (*.zip)', 'All files (*.*)')
    result = window_config_obj.window.create_file_dialog(webview.OPEN_DIALOG, allow_multiple=True, file_types=file_types)
    if not result:
        return
    for i in result:
      print(i)
      i = str(i)
      # 创建文件夹
      name = os.path.basename(i)
      if "." in name:
          name = name.split(".")[0]
      os.mkdir(f'./sounds/{name}')
      shutil.unpack_archive(i, f'./sounds/{name}')
      return name

# 上传音效
def upload_sound():
    sound_name = global_config_obj.choose_sound
    shutil.make_archive(f"tmp/{sound_name}", 'zip', f'./sounds/{sound_name}')
    resp = upload_file(f"tmp/{sound_name}.zip")
    print(json.dumps(resp))
    try:
        url = resp["downloadUrl"]
        print(resp)
        r = upload_plugin(sound_name, "1.0.0", url)
        if r["code"] == 200:
            return True
        else:
            return False
    except Exception as e:
        print(e)
        return False

    return True
    


def play_sound(path: str):
    playsound.playsound(path)

def single_key_switch(key, flag):
    sound_config_obj = SoundConfig.read_from_file(global_config_obj.choose_sound)
    if flag:
        if key not in sound_config_obj.single_key:
            sound_config_obj.single_key.append(key)
    else:
        if key in sound_config_obj.single_key:
            sound_config_obj.single_key.remove(key)
    sound_config_obj.save()

# 播放音效
def playSound(key):
    print(global_config_obj.keyboard_flag)
    print(global_config_obj.choose_sound)
    if global_config_obj.keyboard_flag == False:
        return
    if global_config_obj.choose_sound == "":
        return
    # 读取index.json
    sound_config_obj = SoundConfig.read_from_file(global_config_obj.choose_sound)
    print(sound_config_obj)
    print("当前模式:", sound_config_obj.mode)
    sound_folder = f"./sounds/{global_config_obj.choose_sound}/sounds"
    # 指定模式
    if sound_config_obj.mode == '指定':
        for sound in sound_config_obj.assigned_sounds:
            if sound['key'] == key:
                play_sound(f'{sound_folder}/{sound["sound"]}')
    # 重复模式
    elif sound_config_obj.mode == '重复':
        play_sound(f'{sound_folder}/{sound_config_obj.repeat_sound}')
    # 随机模式
    elif sound_config_obj.mode == '随机':
        soundsList = os.listdir(sound_folder)
        sound = random.choice(soundsList)
        print('随机播放音效：', sound)
        play_sound(f'{sound_folder}/{sound}')
        print('播放完毕------------')
    elif sound_config_obj.mode == '单键随机':
        print(sound_config_obj.single_key)
        if key in sound_config_obj.single_key:
            soundsList = os.listdir(sound_folder)
            sound = random.choice(soundsList)
            print('随机播放音效：', sound)
            play_sound(f'{sound_folder}/{sound}')
            print('播放完毕------------')
            
    if global_config_obj.break_flag:
        name = multiprocessing.current_process().name
        for i, s in enumerate(all_sounds):
            if s[0] == name:
                del all_sounds[i]
    global_config_obj.now_play = False
    

# 获取音效列表
def getSoundList():
    soundsList = os.listdir('./sounds')
    return soundsList

# 获取音效信息
def getSoundInfo(soundName):
    print("soundName:", soundName)
    if not soundName:
        return
    # 读取index.json
    json_data = json.load(open(f'./sounds/{soundName}/index.json', 'r', encoding='utf-8'))
    json_data['soundsList'] = os.listdir(f'./sounds/{soundName}/sounds')
    return json_data

# 更新音效信息
def updateSoundInfo(soundInfo):
   name = soundInfo['name']
   # 读取index.json
   json_data = open(f'./sounds/{name}/index.json', 'w', encoding='utf-8')
   json.dump(soundInfo, json_data, ensure_ascii=False, indent=4)

# 新建音效信息
def newSoundInfo(name):
    print(name)
    SoundConfig.create_sound(name)

# 删除音效信息
def delSoundInfo(name):
    shutil.rmtree(f"./sounds/{name}")
    return True

# 添加音效文件
def addSoundFile(name):
    file_types = ('Image Files (*.mp3;*.wav)', 'All files (*.*)')
    result = window_config_obj.window.create_file_dialog(webview.OPEN_DIALOG, allow_multiple=True, file_types=file_types)
    if result:
      print(result)
      for file in result:
        shutil.copy(file, f'./sounds/{name}/sounds/')
      return True

# 删除音效文件
def delSoundFile(name):
    os.remove(f'./sounds/{global_config_obj.choose_sound}/sounds/{name}')
    return True

# 选择音效
def selectSound(name):
    global_config_obj.choose_sound = name
    # 存入本地sound.ini
    global_config_obj.save()
