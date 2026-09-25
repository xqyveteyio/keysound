# -*- coding:utf-8 _*-
# 修 playsound 1.2.2 在中文 Windows 上放不出声的毛病。
#
# 它内部用的是 mciSendStringA（ANSI 接口），但命令字符串却是按
# sys.getfilesystemencoding() 编码的——Python 3.6 之后这个函数在 Windows 上一律返回
# 'utf-8'，而 ANSI 接口认的是系统代码页（简体中文是 936/GBK）。路径里只要有中文，
# MCI 就报 275「找不到指定的文件」；紧接着它去解码错误信息时又按 utf-8 解 GBK 字节，
# 于是真正的错误被一个 UnicodeDecodeError 盖掉了，看到的是：
#
#     UnicodeDecodeError: 'utf-8' codec can't decode byte 0xd5 in position 0
#
# 三处编码改成 'mbcs'（就是系统代码页）即可。这个脚本会找到**当前这个 Python** 装的
# playsound，备份一份再改，改过了就不重复改。
#
# 用法：
#     python fix_playsound.py            打补丁
#     python fix_playsound.py --restore  从备份还原
#
# 装到新的虚拟环境里之后要重新跑一次，因为改的是 site-packages 里的文件。
import importlib.util
import os
import shutil
import sys

# (原文, 改成什么)，都是 playsound.py 里 winCommand 内部那几行
PATCHES = (
    ("command = ' '.join(command).encode(getfilesystemencoding())",
     "command = ' '.join(command).encode('mbcs')"),
    ("command.decode() +",
     "command.decode('mbcs', 'replace') +"),
    ("errorBuffer.value.decode())",
     "errorBuffer.value.decode('mbcs', 'replace'))"),
)

PATCHED_MARK = "encode('mbcs')"


def find_playsound():
    spec = importlib.util.find_spec('playsound')
    if spec is None or not spec.origin:
        return ''
    return spec.origin


def restore(path):
    backup = path + '.bak'
    if not os.path.isfile(backup):
        print('没有找到备份:', backup)
        return 1
    shutil.copyfile(backup, path)
    print('已还原:', path)
    return 0


def patch(path):
    with open(path, encoding='utf-8') as f:
        source = f.read()

    if PATCHED_MARK in source:
        print('已经打过补丁了，不用再改:', path)
        return 0

    # 版本对不上就别乱改，半途改一半比不改更麻烦
    missing = [old for old, _ in PATCHES if old not in source]
    if missing:
        print('这个 playsound 和预期的 1.2.2 不一样，下面这些代码没找到，没有改动任何东西：')
        for item in missing:
            print('   ', item)
        return 1

    backup = path + '.bak'
    if not os.path.isfile(backup):
        shutil.copyfile(path, backup)
        print('已备份:', backup)

    for old, new in PATCHES:
        source = source.replace(old, new)

    try:
        with open(path, 'w', encoding='utf-8') as f:
            f.write(source)
    except PermissionError:
        print('写不进去，多半是没权限:', path)
        print('用管理员身份再跑一次，或者把 playsound 装到虚拟环境里')
        return 1

    compile(source, path, 'exec')
    print('改好了:', path)
    return 0


def main():
    # Actions 上的 Windows 控制台是 cp1252。中文 print 一抛异常，后面的改写就不会执行
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, 'reconfigure'):
            try:
                stream.reconfigure(encoding='utf-8', errors='replace')
            except (OSError, ValueError):
                pass

    if sys.platform != 'win32':
        print('这个补丁只有 Windows 需要，当前平台是', sys.platform)
        return 0

    path = find_playsound()
    if not path:
        print('当前这个 Python 里没装 playsound：', sys.executable)
        print('先 pip install -r requirements.txt')
        return 1

    print('Python  :', sys.executable)
    print('playsound:', path)

    if '--restore' in sys.argv:
        return restore(path)
    return patch(path)


if __name__ == '__main__':
    sys.exit(main())
