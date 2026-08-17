# keysound

给键盘绑定音效的小工具，原本只支持 Windows，现在 Linux 也能跑（功能有阉割，见下）。

欢迎提交pr

## 运行

```bash
python main.py
```

注意要在项目根目录下运行，配置文件（`config.json`）和音效包（`sounds/`）都是按相对路径找的。

## 安装依赖

### Windows

```bat
python -m venv venv
venv\Scripts\activate
pip install -r requirements.txt
```

窗口走 pywebview 默认的 EdgeChromium 内核（WebView2），Win10 1803 之后系统自带，不用额外装
Qt；万一提示找不到内核，装一下微软的 [WebView2 运行时](https://developer.microsoft.com/microsoft-edge/webview2/)。

### Linux

窗口用 pywebview 的 Qt(QtWebEngine) 内核，PyQt6 有现成的轮子，pip 装就行。
音效走系统的 GStreamer，那部分需要发行版自带的 `gi`，所以虚拟环境要建成能看到系统包的：

```bash
# Fedora
sudo dnf install python3-gobject gstreamer1-plugins-good gstreamer1-plugins-ugly-free
# Debian / Ubuntu
sudo apt install python3-gi gstreamer1.0-plugins-good gstreamer1.0-plugins-ugly

python3 -m venv --system-site-packages venv
source venv/bin/activate
pip install -r requirements.txt
```

音效播放优先用系统的 GStreamer（`gi`），没有的话会退回命令行播放器，装任意一个即可：`ffmpeg`(ffplay) / `mpv` / `mpg123` / `vlc`。

托盘图标是 `QSystemTrayIcon`，走 StatusNotifierItem，**GNOME 需要装 AppIndicator 扩展**才能看到图标；托盘建不起来的话程序会退化成关闭窗口即退出。

把 `utils/platform_util.py` 里的 `WEBVIEW_GUI` 改成 `'gtk'` 可以换成 GTK + WebKit2 内核（托盘会跟着换成 AppIndicator），
额外需要 `webkit2gtk4.1`、`libayatana-appindicator-gtk3`（Debian 系是 `gir1.2-webkit2-4.1`、`gir1.2-ayatanaappindicator3-0.1`）。
不过 GTK 这条路有两个上游问题：Wayland 下 WebKit 的 dmabuf 渲染和 GTK3 的 shm 缓冲会撞显式同步协议，
启动几秒就 `Error 71 (Protocol error)` 退出（代码里靠 `WEBKIT_DISABLE_DMABUF_RENDERER=1` 绕开）；
而且 pywebview 的 GTK 后端把 `evaluate_javascript` 的长度按字符数传给 WebKit，
注入的脚本里有中文就会被截断，键盘监听注入不进去。所以默认用 Qt。

## 音效包

一个音效包就是 `sounds/` 下的一个文件夹：

```
sounds/钢琴块/
├── index.json   # 播放模式、按键绑定等配置
└── sounds/      # mp3 / wav 音频文件
```

界面上的“导出包”把这个文件夹打成单个 `.bspack` 文件（内容就是 zip，换个后缀方便分发和做文件关联），
“导入包”认 `.bspack`，也兼容老版本导出的 `.zip`，还能一次选多个。
导入时包名取文件名，重名自动加 `-2`、`-3`。

## 虚拟麦克风输出（仅 Windows）

设置页里的「虚拟麦克风输出」打开之后，音效除了从音箱出来，还会同时混进你选定的那只麦克风的
信号里，Discord / 语音通话 / 游戏里的其他人也能听到，而且他们那边不用换设备，人声和音效天然
混在一起。

做法和 Soundpad 一样：Windows 上凭空多出一个录音设备必须有内核驱动（要 EV 证书 + 微软签名），
所以这里不创建虚拟设备，而是把一个用户态的**系统效果 APO** 挂到你已有的麦克风端点上，由它在
采集流里把音频加进去。设计细节全在 `实现方案.md` 里。

### 先把两个 C++ 组件编出来

| 文件 | 作用 |
| --- | --- |
| `KeySoundApo.dll` | APO 本体，跑在 `audiodg.exe` 里做混音 |
| `vmic_setup.exe` | 装/卸工具，要管理员权限，负责改注册表和重启音频服务 |

本机装了 Visual Studio 2022（「使用 C++ 的桌面开发」工作负载即可，**不需要 WDK**）：

```bat
msbuild native\KeySoundApo\KeySoundApo.vcxproj /p:Configuration=Release /p:Platform=x64
msbuild native\vmic_setup\vmic_setup.vcxproj /p:Configuration=Release /p:Platform=x64
```

不想装 VS 的话，push 上去让 GitHub Actions 编（`.github/workflows/build-apo.yml`），
从 Actions 页面下载 `KeySoundApo-x64` 这个产物。

编好的两个文件放到 KeySound 根目录就行（`native\...\x64\Release\` 里的产物也能被自动找到，
方便开发时来回改）。

### 用法和注意事项

1. 设置页选一只麦克风，打开开关，会弹一次 UAC，全系统音频会中断两三秒（要重启音频服务）
2. 语音软件里**不用改任何设置**，还是选原来那只麦克风
3. 听不到的话把「挂载槽位」往前换（EFX → MFX → SFX）：语音软件用 RAW 模式打开麦克风时会
   绕过靠后的槽位，哪个能用因机器而异
4. 关掉开关会按备份把注册表恢复原样，备份在 `%APPDATA%\KeySound\vmic-backup\`

**代价**：DLL 没有代码签名，所以装的时候会设 `DisableProtectedAudioDG=1`，受 DRM 保护的音频
（部分流媒体）会播不了；关掉开关时这个值会被删掉。另外共享内存只授权给交互用户，但本机其它
用户态程序理论上仍然能往这只麦克风注入音频，这是这套设计的固有代价。

### 麦克风坏掉了怎么救

1. 声音设置 → 那只麦克风 → 属性 → 关掉「音频增强」，这一步能一键屏蔽所有 APO
2. 还不行：管理员 PowerShell 里删掉端点 `FxProperties` 下值为
   `{F8E20E3A-DD0C-4119-9331-924D5E087417}` 的那一项，然后 `Restart-Service audiosrv -Force`
3. 最后一招：设备管理器卸载声卡驱动后重启，Windows 会装回通用驱动（也会清掉我们的配置）

## 前端

前端在 `web/`（Vue3 + Element Plus），包管理用 yarn 4，构建产物直接输出到根目录的 `ui/`：

```bash
cd web
corepack enable
yarn install
yarn build
```

开发模式见 `web/README.md`。

## Linux 与 Windows 的差异

| | Windows | Linux |
| --- | --- | --- |
| 键盘监听 | 全局钩子（`keyboard`），后台也能出声 | 只监听 KeySound 窗口内的按键，窗口失焦就不响 |
| 音效播放 | `playsound` | GStreamer，找不到就用命令行播放器 |
| 托盘图标 | `pystray` | `QSystemTrayIcon`（GNOME 需要 AppIndicator 扩展） |
| 关闭窗口 | 销毁窗口，托盘点“显示”重建 | 只隐藏窗口（`webview.start()` 不能重复调用），托盘点“显示”恢复 |
| 鼠标音效 | `pynput` | 不支持 |
| 停止播放 | 不支持（`playsound` 没有中止接口） | 支持，掐掉正在响的所有音效 |
| 停止快捷键 | 同上，设了也没用 | 只在 KeySound 窗口聚焦时能触发（和键盘监听同一个限制） |
| 开机自启 | 写注册表 | 写 `~/.config/autostart/keysound.desktop` |
| 虚拟麦克风输出 | 支持，把音效混进选定麦克风的采集流 | 不支持（设置页里不显示这个开关） |

Wayland 出于安全设计不给应用做全局键盘监听（X11 下理论上可以，但需要 root 读 `/dev/input`），所以 Linux 版退化成由网页把窗口内的 `keydown`/`keyup` 回传给 Python 播放音效，代码在 `utils/web_key.py`。
