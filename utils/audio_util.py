# -*- coding:utf-8 _*-
# 音频播放后端。
# Windows 继续用 playsound（保持原样）；Linux 上 playsound 早就没人维护了，
# 优先走系统自带的 GStreamer（gi），没有再退回命令行播放器。
import os
import shutil
import subprocess
import threading

from utils.platform_util import IS_WINDOWS

# 命令行播放器候选：(可执行文件, 固定参数, 支持的后缀)，后缀为 None 表示什么格式都能放
_CLI_PLAYERS = (
    ('ffplay', ['-nodisp', '-autoexit', '-loglevel', 'quiet'], None),
    ('mpv', ['--no-video', '--really-quiet'], None),
    ('gst-play-1.0', ['--quiet'], None),
    ('cvlc', ['--intf', 'dummy', '--play-and-exit', '--quiet'], None),
    ('mpg123', ['-q'], ('.mp3',)),
    ('paplay', [], ('.wav', '.ogg', '.flac')),
    ('pw-play', [], ('.wav', '.ogg', '.flac')),
    ('aplay', ['-q'], ('.wav',)),
)

_backend_lock = threading.Lock()
_gst = None
_gst_checked = False

# 正在播放的声音，打断模式下用来掐掉最早的那个
_playing = []
_playing_lock = threading.Lock()


class _GstSound:
    def __init__(self, gst, path):
        self._gst = gst
        self._stopped = threading.Event()
        self._pipeline = gst.ElementFactory.make('playbin', None)
        self._pipeline.set_property('uri', gst.filename_to_uri(path))
        self._pipeline.set_state(gst.State.PLAYING)

    def wait(self):
        gst = self._gst
        bus = self._pipeline.get_bus()
        end_of_sound = gst.MessageType.EOS | gst.MessageType.ERROR
        # 不用 CLOCK_TIME_NONE 死等，这样 stop() 才能及时把播放掐掉
        while not self._stopped.is_set():
            if bus.timed_pop_filtered(100 * gst.MSECOND, end_of_sound):
                break
        self._pipeline.set_state(gst.State.NULL)

    def stop(self):
        self._stopped.set()


class _ProcessSound:
    def __init__(self, argv):
        self._process = subprocess.Popen(
            argv,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )

    def wait(self):
        self._process.wait()

    def stop(self):
        if self._process.poll() is None:
            self._process.terminate()


def _get_gst():
    global _gst, _gst_checked
    with _backend_lock:
        if not _gst_checked:
            _gst_checked = True
            try:
                import gi
                gi.require_version('Gst', '1.0')
                from gi.repository import Gst
                Gst.init(None)
                _gst = Gst
            except Exception as e:
                print('GStreamer 不可用，改用命令行播放器:', e)
        return _gst


def _find_cli_player(path):
    suffix = os.path.splitext(path)[1].lower()
    for name, args, suffixes in _CLI_PLAYERS:
        if suffixes and suffix not in suffixes:
            continue
        executable = shutil.which(name)
        if executable:
            return [executable, *args, path]
    return None


def _create_sound(path):
    gst = _get_gst()
    if gst:
        return _GstSound(gst, path)
    argv = _find_cli_player(path)
    if argv is None:
        print('找不到可用的音频播放器（装一个 ffmpeg / mpv / mpg123 就行）:', path)
        return None
    return _ProcessSound(argv)


def _register(sound, max_concurrent):
    with _playing_lock:
        while max_concurrent > 0 and len(_playing) >= max_concurrent:
            _playing.pop(0).stop()
        _playing.append(sound)


def _unregister(sound):
    with _playing_lock:
        if sound in _playing:
            _playing.remove(sound)


# 掐掉所有正在响的声音，返回掐掉了几个。
# Windows 走的是 playsound，它没有中止接口，所以这个函数在 Windows 上是空转
def stop_all():
    with _playing_lock:
        # 先摘出来再 stop，别在锁里等 stop
        sounds = list(_playing)
        _playing.clear()
    for sound in sounds:
        try:
            sound.stop()
        except Exception as e:
            print('停止播放出错:', e)
    return len(sounds)


# 播放一个音频文件，播完（或被打断）才返回
# max_concurrent 大于 0 时限制同时播放的数量，超了就掐掉最早的那个
def play_file(path: str, max_concurrent: int = 0):
    path = os.path.abspath(path)
    if not os.path.isfile(path):
        print('音频文件不存在:', path)
        return

    if IS_WINDOWS:
        import playsound
        playsound.playsound(path)
        return

    sound = _create_sound(path)
    if sound is None:
        return
    _register(sound, max_concurrent)
    try:
        sound.wait()
    except Exception as e:
        print('播放音频出错:', e)
    finally:
        _unregister(sound)
