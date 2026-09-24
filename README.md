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

装完依赖还要跑一次：

```bat
python fix_playsound.py
```

playsound 1.2.2 内部用的是 ANSI 的 `mciSendStringA`，命令字符串却按 UTF-8 编码，所以路径里
只要有中文（音效包名基本都是中文）就放不出声，报的还是个把真实错误盖掉的 `UnicodeDecodeError`。
这个脚本会找到**当前这个 Python** 装的 playsound，备份一份再把编码改成系统代码页。
换虚拟环境要重跑；想还原用 `python fix_playsound.py --restore`。

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

## 虚拟麦克风输出

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
