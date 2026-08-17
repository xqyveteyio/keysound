# keysound

给键盘绑定音效的小工具，原本只支持 Windows，现在 Linux 也能跑（功能有阉割，见下）。

欢迎提交pr

## 运行

```bash
python KeySound.py
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

pywebview 在 Linux 上用的是 GTK(WebKit2) 内核，PyGObject 从源码编译很麻烦，建议直接用发行版自带的包，再建一个能看到系统包的虚拟环境：

```bash
# Fedora
sudo dnf install python3-gobject python3-cairo webkit2gtk4.1 libayatana-appindicator-gtk3 \
    gstreamer1-plugins-good gstreamer1-plugins-ugly-free
# Debian / Ubuntu
sudo apt install python3-gi python3-cairo gir1.2-webkit2-4.1 gir1.2-ayatanaappindicator3-0.1 \
    gstreamer1.0-plugins-good gstreamer1.0-plugins-ugly

python3 -m venv --system-site-packages venv
source venv/bin/activate
pip install -r requirements.txt
```

音效播放优先用系统的 GStreamer（`gi`），没有的话会退回命令行播放器，装任意一个即可：`ffmpeg`(ffplay) / `mpv` / `mpg123` / `vlc`。

托盘图标走的是 AppIndicator(StatusNotifierItem)，**GNOME 需要装 AppIndicator 扩展**才能看到图标；托盘建不起来的话程序会退化成关闭窗口即退出。

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
| 托盘图标 | `pystray` | AppIndicator（GNOME 需要 AppIndicator 扩展） |
| 关闭窗口 | 销毁窗口，托盘点“显示”重建 | 只隐藏窗口（`webview.start()` 不能重复调用），托盘点“显示”恢复 |
| 鼠标音效 | `pynput` | 不支持 |
| 开机自启 | 写注册表 | 写 `~/.config/autostart/keysound.desktop` |

Wayland 出于安全设计不给应用做全局键盘监听（X11 下理论上可以，但需要 root 读 `/dev/input`），所以 Linux 版退化成由网页把窗口内的 `keydown`/`keyup` 回传给 Python 播放音效，代码在 `utils/web_key.py`。
