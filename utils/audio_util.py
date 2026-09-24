# -*- coding:utf-8 _*-
# 音频播放。playsound 没有中止接口，停止播放在这里是空操作。
# 麦克风那一路的打断在 vmic_win.clear()，不走这里。
import os


# 掐掉正在响的声音。playsound 停不掉，所以这里直接返回 0。
def stop_all():
    return 0


# 播放一个音频文件，播完才返回。
# max_concurrent 留着是为了兼容原来的调用，playsound 做不到中途掐掉。
def play_file(path: str, max_concurrent: int = 0):
    del max_concurrent
    path = os.path.abspath(path)
    if not os.path.isfile(path):
        print('音频文件不存在:', path)
        return
    import playsound
    playsound.playsound(path)
