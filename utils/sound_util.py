from config.global_config import global_config_obj
from config.sound_config import SoundConfig
from utils import audio_util
from utils import vmic_win
import random
import os
import json
import shutil
import tempfile
import webview
from config.window_config import window_config_obj
import multiprocessing
from config.global_config import all_sounds, global_config_obj
from utils.api import upload_file, upload_plugin


# 音效包的分发格式：一个包一个文件。内容就是 zip（里面是 index.json + sounds/），
# 换个后缀是为了让人一眼知道这是 KeySound 的包，也方便做文件关联
PACK_EXT = '.bspack'
# 老版本导出的是 .zip，导入时一并认
PACK_FILE_TYPES = (
    f'KeySound 音效包 (*{PACK_EXT})',
    'Zip 压缩包 (*.zip)',
    'All files (*.*)',
)


# 导出音效包：打包成单个 .bspack 文件
def exportSound(name):
    result = window_config_obj.window.create_file_dialog(
        webview.SAVE_DIALOG, allow_multiple=False,
        file_types=PACK_FILE_TYPES, save_filename=f'{name}{PACK_EXT}')
    if not result:
        return False
    target = result if isinstance(result, str) else result[0]
    # Qt 的保存框不会照着筛选器自动补后缀，用户删了得给他加回来
    if os.path.splitext(target)[1].lower() not in (PACK_EXT, '.zip'):
        target += PACK_EXT
    # make_archive 只认 .zip 后缀（自己往 base_name 后面接），
    # 没法直接产出 .bspack，所以先打到临时目录再挪过去
    tmp_dir = tempfile.mkdtemp(prefix='keysound-pack-')
    try:
        zip_path = shutil.make_archive(
            os.path.join(tmp_dir, 'pack'), 'zip', f'./sounds/{name}')
        shutil.move(zip_path, target)
    finally:
        shutil.rmtree(tmp_dir, ignore_errors=True)
    print('导出音效包:', target)
    return True


# 导入音效包：可以一次选多个，返回真正导入进来的包名列表；取消返回 None
def importSound():
    result = window_config_obj.window.create_file_dialog(
        webview.OPEN_DIALOG, allow_multiple=True, file_types=PACK_FILE_TYPES)
    if not result:
        return None
    names = []
    for path in result:
        try:
            names.append(_import_pack(str(path)))
        except Exception as e:
            # 一个包坏了不影响剩下的
            print('导入失败:', path, e)
    return names


def _import_pack(path):
    # 包名取文件名（只去掉最后一个后缀），已经有同名的就往后加 -2 -3
    base = os.path.splitext(os.path.basename(path))[0] or '音效包'
    name = base
    i = 2
    while os.path.exists(f'./sounds/{name}'):
        name = f'{base}-{i}'
        i += 1
    dest = f'./sounds/{name}'
    os.makedirs(dest)
    try:
        # 后缀不是 .zip，unpack_archive 猜不出格式，直接告诉它
        shutil.unpack_archive(path, dest, format='zip')
        _flatten_single_dir(dest)
        _fix_pack_name(name, dest)
    except Exception:
        # 解压到一半失败就别留个残包在列表里
        shutil.rmtree(dest, ignore_errors=True)
        raise
    print('导入音效包:', path, '->', name)
    return name


# 有些包是连着外层文件夹一起压的（zip 里是 包名/index.json），这种拆掉外层
def _flatten_single_dir(dest):
    entries = os.listdir(dest)
    if len(entries) != 1:
        return
    inner = os.path.join(dest, entries[0])
    if not os.path.isfile(os.path.join(inner, 'index.json')):
        return
    for item in os.listdir(inner):
        shutil.move(os.path.join(inner, item), os.path.join(dest, item))
    os.rmdir(inner)


# index.json 里的 name 是导出方的包名，和这边的文件夹名不一致的话，
# 之后改配置、删包都会照着 name 去找路径，找错就报错或者删错东西
def _fix_pack_name(name, dest):
    index_path = os.path.join(dest, 'index.json')
    if not os.path.isfile(index_path):
        raise ValueError('不是 KeySound 音效包（缺 index.json）')
    with open(index_path, encoding='utf-8') as f:
        info = json.load(f)
    info['name'] = name
    with open(index_path, 'w', encoding='utf-8') as f:
        json.dump(info, f, ensure_ascii=False, indent=4)
    # 空包导出时可能没有 sounds 目录，补上，不然后面添加音效会失败
    os.makedirs(os.path.join(dest, 'sounds'), exist_ok=True)

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
    # 虚拟麦克风那一路要在播放之前喂：play_file() 是阻塞到播完才返回的，
    # 放在后面等于每个音效都晚一整段时间才进麦克风
    vmic_win.feed(path)
    # 打断模式下最多同时响 3 个音，和 Windows 那边多进程打断的逻辑保持一致
    audio_util.play_file(path, max_concurrent=3 if global_config_obj.break_flag else 0)

# 试听单个音效文件：音效列表里点一下文件名就走这里
def previewSound(name):
    pack = global_config_obj.choose_sound
    if not pack or not name:
        return False
    # name 是页面传过来的，只取文件名部分，别让它跑出音效包目录
    path = f'./sounds/{pack}/sounds/{os.path.basename(str(name))}'
    if not os.path.isfile(path):
        print('音效文件不存在:', path)
        return False
    play_sound(path)
    return True


# 立刻停掉所有正在响的音效，按键触发的和试听的都算
def stopSound():
    count = audio_util.stop_all()
    # 环形缓冲里剩下的那点音频不清掉的话，还会继续从麦克风出去
    vmic_win.clear()
    print('停止播放，掐掉', count, '个音效')
    return count


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
