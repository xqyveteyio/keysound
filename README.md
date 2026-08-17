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

也可以用 `KEYSOUND_WEBVIEW_GUI=gtk` 换成 GTK + WebKit2 内核（托盘会跟着换成 AppIndicator），
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
| 开机自启 | 写注册表 | 写 `~/.config/autostart/keysound.desktop` |

Wayland 出于安全设计不给应用做全局键盘监听（X11 下理论上可以，但需要 root 读 `/dev/input`），所以 Linux 版退化成由网页把窗口内的 `keydown`/`keyup` 回传给 Python 播放音效，代码在 `utils/web_key.py`。
