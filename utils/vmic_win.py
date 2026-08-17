# -*- coding:utf-8 _*-
# 虚拟麦克风输出（Windows 专用）。
#
# 思路见 实现方案.md：Windows 上没法凭空造一个录音设备（那要内核驱动 + EV 证书），
# 所以走 Soundpad 那条路——把一个用户态的系统效果 APO 挂到用户已有的那只麦克风上，
# 由它在采集流里把我们的音效加进去。这个文件负责 Python 这一侧的三件事：
#   1. 枚举录音端点、拉起提权的 vmic_setup.exe 装/卸 APO
#   2. 把音效解码成端点的格式，多路相加，写进和 APO 共享的环形缓冲
#   3. 报状态（APO 到底加载起来没有、注册表配置还在不在）
#
# 共享内存由 APO 创建、这边只打开：KeySound 是普通用户权限，没有
# SeCreateGlobalPrivilege，建不了 Global\ 命名空间的对象；而建在 Local\ 里的话，
# 跑在会话 0 的 audiodg.exe 又看不见。
import ctypes
import json
import os
import queue
import tempfile
import threading
import time
from array import array

from utils.platform_util import IS_WINDOWS

if IS_WINDOWS:
    import winreg

# ---------------------------------------------------------------- 常量
# 下面这些必须和 native/common/KeySoundShared.h 保持一致

RING_NAME = 'Global\\KeySoundApoRing'
RING_MAGIC = 0x3141534B
RING_HEADER_SIZE = 64
RING_MAX_FRAMES = 19200
RING_MAX_CHANNELS = 8

APO_CLSID = '{F8E20E3A-DD0C-4119-9331-924D5E087417}'

CAPTURE_ROOT = r'SOFTWARE\Microsoft\Windows\CurrentVersion\MMDevices\Audio\Capture'
# 槽位对应的 FxProperties 值名，离硬件由近到远是 efx -> mfx -> sfx，
# RAW 模式下越靠近硬件越有可能还生效，所以默认挂 efx
SLOT_VALUES = {
    'efx': '{d04e05a6-594b-4fb6-a80d-01af5eed7d1d},7',
    'mfx': '{d04e05a6-594b-4fb6-a80d-01af5eed7d1d},6',
    'sfx': '{d04e05a6-594b-4fb6-a80d-01af5eed7d1d},5',
}
PROP_FRIENDLY_NAME = '{a45c254e-df1c-4efd-8020-67d146a850e0},14'
PROP_DEVICE_DESC = '{a45c254e-df1c-4efd-8020-67d146a850e0},2'
PROP_INTERFACE_NAME = '{b3f8fa53-0004-438e-9003-51a46e139bfc},6'

DEVICE_STATE_ACTIVE = 1

# 缓冲里保持多少毫秒的余量。太小容易断音，太大则「停止播放」之后残留还会继续响
TARGET_MS = 40
# 混音线程的节拍
TICK_SECONDS = 0.01
# 解码结果缓存多少条。同一个音效反复按很常见，缓存收益很大
DECODE_CACHE_SIZE = 48


def is_supported():
    return IS_WINDOWS


# ---------------------------------------------------------------- Win32 绑定

if IS_WINDOWS:
    _kernel32 = ctypes.WinDLL('kernel32', use_last_error=True)
    _kernel32.OpenFileMappingW.restype = ctypes.c_void_p
    _kernel32.OpenFileMappingW.argtypes = [ctypes.c_uint32, ctypes.c_int, ctypes.c_wchar_p]
    _kernel32.MapViewOfFile.restype = ctypes.c_void_p
    _kernel32.MapViewOfFile.argtypes = [ctypes.c_void_p, ctypes.c_uint32,
                                        ctypes.c_uint32, ctypes.c_uint32, ctypes.c_size_t]
    _kernel32.UnmapViewOfFile.argtypes = [ctypes.c_void_p]
    _kernel32.CloseHandle.argtypes = [ctypes.c_void_p]
    _kernel32.WaitForSingleObject.argtypes = [ctypes.c_void_p, ctypes.c_uint32]

    FILE_MAP_WRITE = 0x0002
    FILE_MAP_READ = 0x0004


    class _RingHeader(ctypes.Structure):
        _pack_ = 4
        _fields_ = [
            ('magic', ctypes.c_uint32),
            ('version', ctypes.c_uint32),
            ('sample_rate', ctypes.c_uint32),
            ('channels', ctypes.c_uint32),
            ('capacity', ctypes.c_uint32),
            ('apo_alive', ctypes.c_uint32),
            ('write_index', ctypes.c_uint32),
            ('read_index', ctypes.c_uint32),
            ('consumer_claimed', ctypes.c_uint32),
            ('reserved', ctypes.c_uint32 * 7),
        ]


# ---------------------------------------------------------------- 环形缓冲（生产者端）

class _Ring:
    def __init__(self):
        self.handle = None
        self.base = None
        self.header = None

    @property
    def opened(self):
        return self.header is not None

    # APO 只有在有程序真的在用麦克风的时候才会被实例化，所以打不开是常态，不要当成错误
    def open(self):
        self.close()
        handle = _kernel32.OpenFileMappingW(FILE_MAP_READ | FILE_MAP_WRITE, False, RING_NAME)
        if not handle:
            return False
        base = _kernel32.MapViewOfFile(handle, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, 0)
        if not base:
            _kernel32.CloseHandle(handle)
            return False
        header = _RingHeader.from_address(base)
        if header.magic != RING_MAGIC or header.capacity == 0 or header.channels == 0:
            _kernel32.UnmapViewOfFile(ctypes.c_void_p(base))
            _kernel32.CloseHandle(handle)
            return False
        self.handle = handle
        self.base = base
        self.header = header
        return True

    def close(self):
        if self.base:
            _kernel32.UnmapViewOfFile(ctypes.c_void_p(self.base))
        if self.handle:
            _kernel32.CloseHandle(ctypes.c_void_p(self.handle))
        self.handle = None
        self.base = None
        self.header = None

    def healthy(self):
        return self.header is not None and self.header.magic == RING_MAGIC

    # 把一段交织的 float32 写进去，返回实际写进去了几帧。
    # 只有这边动 write_index，只有 APO 动 read_index，所以不需要锁
    def write(self, block, frames):
        header = self.header
        capacity = header.capacity
        channels = header.channels
        read = header.read_index % capacity
        write = header.write_index % capacity
        free = capacity - ((write - read + capacity) % capacity) - 1
        if free <= 0:
            return 0
        if frames > free:
            frames = free

        raw = array('f', block[:frames * channels]).tobytes()
        first = min(frames, capacity - write)
        ctypes.memmove(self.base + RING_HEADER_SIZE + write * channels * 4,
                       raw, first * channels * 4)
        if frames > first:
            ctypes.memmove(self.base + RING_HEADER_SIZE,
                           raw[first * channels * 4:], (frames - first) * channels * 4)
        # 下标一定要最后写：APO 看到新下标时，数据必须已经在内存里了
        header.write_index = (write + frames) % capacity
        return frames

    def flush(self):
        if self.healthy():
            self.header.write_index = self.header.read_index


# ---------------------------------------------------------------- 混音器

class _Playing:
    __slots__ = ('samples', 'offset')

    def __init__(self, samples):
        self.samples = samples
        self.offset = 0


_ring = _Ring()
_playing = []
_playing_lock = threading.Lock()
_worker = None
_decoder = None
_worker_stop = threading.Event()
# 连按时按键线程不能等解码，扔给一个固定的解码线程；队列满了宁可丢一个音效
_decode_queue = queue.Queue(maxsize=16)
_decode_cache = {}
_decode_cache_order = []
_decode_lock = threading.Lock()
# 最近一次看到 APO 心跳在动的时间，设置页问状态的时候用
_apo_seen_at = 0.0
_last_alive = None


def _config():
    # 延迟导入，避免和 config 包互相 import
    from config.global_config import global_config_obj
    return global_config_obj


def _enabled():
    if not IS_WINDOWS:
        return False
    config = _config()
    return bool(getattr(config, 'virtual_mic', False)) and \
        bool(getattr(config, 'virtual_mic_device', ''))


def _gain():
    try:
        return max(0.0, float(getattr(_config(), 'virtual_mic_volume', 100)) / 100.0)
    except (TypeError, ValueError):
        return 1.0


def _decode(path, sample_rate, channels):
    try:
        import miniaudio
    except ImportError:
        return None

    path = os.path.abspath(path)
    try:
        key = (path, os.path.getmtime(path), sample_rate, channels)
    except OSError:
        return None

    with _decode_lock:
        cached = _decode_cache.get(key)
    if cached is not None:
        return cached

    # 不用 decode_file()：它把文件名按 sys.getfilesystemencoding()（Windows 上是 utf-8）
    # 编码后交给 C 那边的窄字符 fopen，而 fopen 认的是系统代码页，
    # 路径里有中文就报 MA_DOES_NOT_EXIST。自己读成 bytes 再内存解码就没这问题
    try:
        with open(path, 'rb') as f:
            data = f.read()
        decoded = miniaudio.decode(data,
                                   output_format=miniaudio.SampleFormat.FLOAT32,
                                   nchannels=channels,
                                   sample_rate=sample_rate)
    except Exception as e:
        print('虚拟麦克风解码失败:', path, e)
        return None

    samples = decoded.samples
    with _decode_lock:
        _decode_cache[key] = samples
        _decode_cache_order.append(key)
        while len(_decode_cache_order) > DECODE_CACHE_SIZE:
            _decode_cache.pop(_decode_cache_order.pop(0), None)
    return samples


def _decoder_loop():
    while not _worker_stop.is_set():
        try:
            path = _decode_queue.get(timeout=0.5)
        except queue.Empty:
            continue
        if not _ring.healthy():
            continue
        header = _ring.header
        samples = _decode(path, header.sample_rate, header.channels)
        if not samples:
            continue
        with _playing_lock:
            _playing.append(_Playing(samples))


def _mix_and_write():
    global _apo_seen_at, _last_alive

    header = _ring.header
    alive = header.apo_alive
    if _last_alive is not None and alive != _last_alive:
        # 心跳在动才说明 APO 真被 audiodg 加载起来了，段存在只能说明它建过
        _apo_seen_at = time.time()
    _last_alive = alive

    with _playing_lock:
        if not _playing:
            return

    capacity = header.capacity
    channels = header.channels
    read = header.read_index % capacity
    write = header.write_index % capacity
    buffered = (write - read + capacity) % capacity
    target = max(1, int(header.sample_rate * TARGET_MS / 1000))
    need = target - buffered
    if need <= 0:
        return
    if need > target:
        need = target

    count = need * channels
    block = [0.0] * count
    gain = _gain()

    with _playing_lock:
        finished = []
        for item in _playing:
            samples = item.samples
            left = len(samples) - item.offset
            if left <= 0:
                finished.append(item)
                continue
            take = count if count < left else left
            chunk = samples[item.offset:item.offset + take]
            for i in range(take):
                block[i] += chunk[i]
            item.offset += take
            if item.offset >= len(samples):
                finished.append(item)
        for item in finished:
            _playing.remove(item)

    # 削波：连按时几个音效相加很容易过 1，直接溢出会变成刺啦声
    for i in range(count):
        value = block[i] * gain
        if value > 1.0:
            value = 1.0
        elif value < -1.0:
            value = -1.0
        block[i] = value

    # 写不下的直接丢掉（上面已经推进过各自的 offset）：宁可丢音频也不能把线程堵住
    _ring.write(block, need)


def _worker_loop():
    last_open_try = 0.0
    while not _worker_stop.is_set():
        if not _enabled():
            # 开关关着的时候没必要每 10ms 醒一次
            if _ring.opened:
                _ring.close()
            time.sleep(0.3)
            continue
        time.sleep(TICK_SECONDS)
        if not _ring.healthy():
            _ring.close()
            now = time.time()
            # 没有程序在用麦克风的时候 APO 根本不会被实例化，别每 10ms 试一次
            if now - last_open_try < 1.0:
                continue
            last_open_try = now
            if not _ring.open():
                continue
        try:
            _mix_and_write()
        except Exception as e:
            print('虚拟麦克风混音出错:', e)
            _ring.close()


def start():
    global _worker, _decoder
    if not IS_WINDOWS or _worker is not None:
        return
    _worker_stop.clear()
    _worker = threading.Thread(target=_worker_loop, name='keysound-vmic', daemon=True)
    _worker.start()
    _decoder = threading.Thread(target=_decoder_loop, name='keysound-vmic-decode', daemon=True)
    _decoder.start()


def stop():
    global _worker, _decoder
    _worker_stop.set()
    _worker = None
    _decoder = None
    clear()
    _ring.close()


# 把一个音效文件喂进虚拟麦克风那一路。
# 解码是耗时的，不能在按键线程里做
def feed(path):
    if not _enabled() or not _ring.healthy():
        return
    try:
        _decode_queue.put_nowait(path)
    except queue.Full:
        pass


# 停止播放时调：缓冲里剩下的音频不清掉的话，还会继续从麦克风出去
def clear():
    while True:
        try:
            _decode_queue.get_nowait()
        except queue.Empty:
            break
    with _playing_lock:
        _playing[:] = []
    if _ring.healthy():
        _ring.flush()


# ---------------------------------------------------------------- 端点枚举

def _read_value(key, name):
    try:
        return winreg.QueryValueEx(key, name)[0]
    except OSError:
        return None


def _endpoint_name(guid):
    try:
        with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE,
                            '%s\\%s\\Properties' % (CAPTURE_ROOT, guid), 0,
                            winreg.KEY_READ | winreg.KEY_WOW64_64KEY) as key:
            friendly = _read_value(key, PROP_FRIENDLY_NAME)
            if friendly:
                return str(friendly)
            desc = _read_value(key, PROP_DEVICE_DESC)
            interface = _read_value(key, PROP_INTERFACE_NAME)
            if desc and interface:
                return '%s (%s)' % (desc, interface)
            return str(desc or interface or guid)
    except OSError:
        return guid


def _endpoint_slot(guid):
    try:
        with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE,
                            '%s\\%s\\FxProperties' % (CAPTURE_ROOT, guid), 0,
                            winreg.KEY_READ | winreg.KEY_WOW64_64KEY) as key:
            for slot, value_name in SLOT_VALUES.items():
                value = _read_value(key, value_name)
                if value and str(value).upper() == APO_CLSID.upper():
                    return slot
    except OSError:
        pass
    return ''


def _default_capture_endpoint():
    # 直接读注册表判断不了哪只是默认设备，只能问一下 MMDeviceEnumerator。
    # 拿不到就算了，界面上退化成让用户自己选
    try:
        ole32 = ctypes.WinDLL('ole32')

        class GUID(ctypes.Structure):
            _fields_ = [('Data1', ctypes.c_uint32), ('Data2', ctypes.c_uint16),
                        ('Data3', ctypes.c_uint16), ('Data4', ctypes.c_ubyte * 8)]

        def guid(text):
            value = GUID()
            if ole32.CLSIDFromString(ctypes.c_wchar_p(text), ctypes.byref(value)) != 0:
                raise OSError('CLSIDFromString 失败')
            return value

        ole32.CoInitializeEx(None, 0)
        enumerator = ctypes.c_void_p()
        hr = ole32.CoCreateInstance(
            ctypes.byref(guid('{BCDE0395-E52F-467C-8E3D-C4579291692E}')), None, 23,
            ctypes.byref(guid('{A95664D2-9614-4F35-A746-DE8DB63617E6}')),
            ctypes.byref(enumerator))
        if hr != 0:
            return ''

        def method(obj, index, *arg_types):
            vtable = ctypes.cast(obj, ctypes.POINTER(ctypes.POINTER(ctypes.c_void_p)))[0]
            proto = ctypes.WINFUNCTYPE(ctypes.c_long, ctypes.c_void_p, *arg_types)
            return proto(vtable[index])

        try:
            device = ctypes.c_void_p()
            # IMMDeviceEnumerator::GetDefaultAudioEndpoint(eCapture=1, eConsole=0, &device)
            hr = method(enumerator, 4, ctypes.c_int, ctypes.c_int,
                        ctypes.POINTER(ctypes.c_void_p))(enumerator, 1, 0, ctypes.byref(device))
            if hr != 0:
                return ''
            try:
                text = ctypes.c_void_p()
                # IMMDevice::GetId(&id)
                hr = method(device, 5, ctypes.POINTER(ctypes.c_void_p))(device, ctypes.byref(text))
                if hr != 0:
                    return ''
                device_id = ctypes.wstring_at(text)
                ole32.CoTaskMemFree(text)
            finally:
                method(device, 2)(device)
        finally:
            method(enumerator, 2)(enumerator)

        # 形如 {0.0.1.00000000}.{端点GUID}，我们要的是最后那个花括号里的
        start = device_id.rfind('{')
        return device_id[start:] if start >= 0 else ''
    except Exception:
        return ''


def list_capture_devices():
    if not IS_WINDOWS:
        return []
    default_id = _default_capture_endpoint()
    devices = []
    try:
        with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, CAPTURE_ROOT, 0,
                            winreg.KEY_READ | winreg.KEY_WOW64_64KEY) as root:
            index = 0
            while True:
                try:
                    guid = winreg.EnumKey(root, index)
                except OSError:
                    break
                index += 1
                state = None
                try:
                    with winreg.OpenKey(root, guid, 0,
                                        winreg.KEY_READ | winreg.KEY_WOW64_64KEY) as key:
                        state = _read_value(key, 'DeviceState')
                except OSError:
                    pass
                devices.append({
                    'id': guid,
                    'name': _endpoint_name(guid),
                    'active': state == DEVICE_STATE_ACTIVE,
                    'default': guid.upper() == default_id.upper() if default_id else False,
                    'slot': _endpoint_slot(guid),
                })
    except OSError as e:
        print('枚举录音设备失败:', e)
    # 拔掉的、禁用的排后面，免得用户在一堆没用的设备里翻
    devices.sort(key=lambda d: (not d['active'], d['name']))
    return devices


# ---------------------------------------------------------------- 装 / 卸

def _app_dir():
    return os.path.abspath('.')


def _find_file(name):
    candidates = [
        os.path.join(_app_dir(), name),
        # 开发时直接用 CI 或者 VS 编出来的产物，省得每次手动往根目录拷
        os.path.join(_app_dir(), 'native', 'KeySoundApo', 'x64', 'Release', name),
        os.path.join(_app_dir(), 'native', 'vmic_setup', 'x64', 'Release', name),
    ]
    for path in candidates:
        if os.path.isfile(path):
            return path
    return ''


def _backup_dir():
    base = os.environ.get('APPDATA') or tempfile.gettempdir()
    return os.path.join(base, 'KeySound', 'vmic-backup')


def _run_elevated(arguments):
    """拉起提权的 vmic_setup.exe 并等它跑完，返回它写在结果文件里的那个 dict。"""
    tool = _find_file('vmic_setup.exe')
    if not tool:
        return {'ok': False, 'message': '找不到 vmic_setup.exe，先按 README 把它编出来放到 KeySound 目录'}

    result_path = os.path.join(tempfile.gettempdir(), 'keysound-vmic-result.json')
    try:
        os.remove(result_path)
    except OSError:
        pass

    arguments = list(arguments) + ['--result', result_path]
    quoted = ' '.join('"%s"' % a if ' ' in a else a for a in arguments)

    class SHELLEXECUTEINFOW(ctypes.Structure):
        _fields_ = [
            ('cbSize', ctypes.c_uint32),
            ('fMask', ctypes.c_ulong),
            ('hwnd', ctypes.c_void_p),
            ('lpVerb', ctypes.c_wchar_p),
            ('lpFile', ctypes.c_wchar_p),
            ('lpParameters', ctypes.c_wchar_p),
            ('lpDirectory', ctypes.c_wchar_p),
            ('nShow', ctypes.c_int),
            ('hInstApp', ctypes.c_void_p),
            ('lpIDList', ctypes.c_void_p),
            ('lpClass', ctypes.c_wchar_p),
            ('hkeyClass', ctypes.c_void_p),
            ('dwHotKey', ctypes.c_uint32),
            ('hIcon', ctypes.c_void_p),
            ('hProcess', ctypes.c_void_p),
        ]

    info = SHELLEXECUTEINFOW()
    info.cbSize = ctypes.sizeof(info)
    info.fMask = 0x00000040  # SEE_MASK_NOCLOSEPROCESS，要拿到进程句柄才能等它
    info.lpVerb = 'runas'
    info.lpFile = tool
    info.lpParameters = quoted
    info.lpDirectory = os.path.dirname(tool)
    info.nShow = 0  # SW_HIDE

    shell32 = ctypes.WinDLL('shell32', use_last_error=True)
    if not shell32.ShellExecuteExW(ctypes.byref(info)):
        code = ctypes.get_last_error()
        if code == 1223:  # ERROR_CANCELLED
            return {'ok': False, 'message': '需要管理员权限，UAC 被取消了'}
        return {'ok': False, 'message': '启动 vmic_setup.exe 失败（错误码 %d）' % code}

    if info.hProcess:
        _kernel32.WaitForSingleObject(info.hProcess, 120000)
        _kernel32.CloseHandle(info.hProcess)

    try:
        with open(result_path, encoding='utf-8-sig') as f:
            return json.loads(f.read())
    except (OSError, ValueError) as e:
        return {'ok': False, 'message': 'vmic_setup.exe 没有返回结果（%s）' % e}


def enable_virtual_mic(device_id='', slot=''):
    if not IS_WINDOWS:
        return {'ok': False, 'message': '这个功能只有 Windows 上有'}

    config = _config()
    device_id = device_id or config.virtual_mic_device
    slot = slot or config.virtual_mic_slot or 'efx'
    if not device_id:
        return {'ok': False, 'message': '先选一只麦克风'}
    if slot not in SLOT_VALUES:
        return {'ok': False, 'message': '槽位只能是 efx / mfx / sfx'}

    dll = _find_file('KeySoundApo.dll')
    if not dll:
        return {'ok': False, 'message': '找不到 KeySoundApo.dll，先按 README 把它编出来放到 KeySound 目录'}

    result = _run_elevated(['install', '--dll', dll, '--endpoint', device_id,
                            '--slot', slot, '--backup', _backup_dir()])
    if result.get('ok'):
        config.virtual_mic = True
        config.virtual_mic_device = device_id
        config.virtual_mic_slot = slot
        config.save()
        start()
    return result


def disable_virtual_mic():
    if not IS_WINDOWS:
        return {'ok': False, 'message': '这个功能只有 Windows 上有'}

    config = _config()
    device_id = config.virtual_mic_device
    # 先把开关关掉再去改注册表：万一 UAC 被取消，界面上的状态也不会停在「开着」
    config.virtual_mic = False
    config.save()
    clear()

    if not device_id:
        return {'ok': True, 'message': '已关闭'}
    return _run_elevated(['uninstall', '--endpoint', device_id, '--backup', _backup_dir()])


def virtual_mic_status():
    if not IS_WINDOWS:
        return {'supported': False}

    config = _config()
    device_id = getattr(config, 'virtual_mic_device', '')
    installed_slot = _endpoint_slot(device_id) if device_id else ''

    # 心跳在动才说明 APO 真的被 audiodg 加载起来了。
    # 混音线程没在跑的时候现场采一次，两次读数不一样就算活着
    alive = False
    if _apo_seen_at and time.time() - _apo_seen_at < 2.0:
        alive = True
    elif _worker is None:
        # 混音线程没在跑（开关是关的）才现场采一次，不然会和它抢同一个映射
        probe = _Ring()
        if probe.open():
            first = probe.header.apo_alive
            time.sleep(0.15)
            alive = probe.healthy() and probe.header.apo_alive != first
            probe.close()

    return {
        'supported': True,
        'enabled': bool(getattr(config, 'virtual_mic', False)),
        'device': device_id,
        'device_name': _endpoint_name(device_id) if device_id else '',
        'slot': getattr(config, 'virtual_mic_slot', 'efx'),
        'installed_slot': installed_slot,
        # 配置说开着、注册表里却没有我们的 CLSID，多半是音频驱动重装或者
        # Windows 更新把配置冲掉了（实现方案.md 第 3 节第 2 条），要提示用户重新挂一次
        'needs_repair': bool(getattr(config, 'virtual_mic', False)) and not installed_slot,
        'apo_running': alive,
        'sample_rate': _ring.header.sample_rate if _ring.healthy() else 0,
        'channels': _ring.header.channels if _ring.healthy() else 0,
        'volume': getattr(config, 'virtual_mic_volume', 100),
        'tool_ready': bool(_find_file('vmic_setup.exe')) and bool(_find_file('KeySoundApo.dll')),
        'decoder_ready': _decoder_ready(),
    }


def _decoder_ready():
    try:
        import miniaudio  # noqa: F401
        return True
    except ImportError:
        return False
