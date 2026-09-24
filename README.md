# keysound

给键盘绑定音效的小工具，只支持 Windows。窗口用系统自带的 Edge WebView2。

欢迎提交pr

## 运行

```bash
python main.py
```

注意要在项目根目录下运行，配置文件（`config.json`）和音效包（`sounds/`）都是按相对路径找的。

## 安装依赖

需要 Windows 10/11，并且装有 [WebView2 运行时](https://developer.microsoft.com/microsoft-edge/webview2/)（Windows 11 和装了 Edge 的 Windows 10 一般已经有了）。

```bat
python -m venv venv
venv\Scripts\activate
pip install -r requirements.txt
```

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

## 行为

| | |
| --- | --- |
| 窗口 | Edge WebView2（`pywebview` 的 `edgechromium`） |
| 键盘监听 | 全局钩子（`keyboard`），后台也能出声 |
| 音效播放 | `playsound` |
| 托盘图标 | `pystray` |
| 关闭窗口 | 销毁窗口，托盘点“显示”重建 |
| 鼠标音效 | `pynput` |
| 停止播放 | 不支持（`playsound` 没有中止接口） |
| 开机自启 | 写注册表 `HKCU\...\Run\KeySound` |
