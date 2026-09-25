; KeySound 安装包。CI 用 ISCC 编，不要再写死本机盘符。
;
; 装到当前用户目录，安装本身不弹 UAC。虚拟麦克风那份 DLL 不能跟程序放一起：
; vmic_setup 会把它复制到 C:\Program Files\KeySound 给 audiodg 加载。
; 如果整个程序也装进那个目录，卸载时会把正在用的 DLL 一起删掉，麦克风就会断。
;
; 快捷方式必须把工作目录指到安装目录。程序是按当前目录找 ui/ 和 themes/ 的。

#ifndef MyAppVersion
  #define MyAppVersion "2.2.0"
#endif

#define MyAppName "KeySound"
#define MyAppPublisher "Bojaka"
#define MyAppURL "https://www.bojaka.cn/"
#define MyAppExeName "KeySound.exe"

[Setup]
; 沿用原来的 AppId，升级会卸掉上一版，不会装出两个 KeySound
AppId={{E6D94B48-7B89-45EB-BD73-9D66E2AE428D}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
; 不跟着旧版的目录走。旧版在 Program Files，新版固定到用户目录
UsePreviousAppDir=no
DefaultDirName={localappdata}\Programs\{#MyAppName}
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=..\dist
OutputBaseFilename=KeySound-Setup
InfoBeforeFile=..\安装前.txt
InfoAfterFile=..\安装后.txt
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
CloseApplications=yes
RestartApplications=no

[Languages]
; 发行版的 ISCC 不一定带简体中文，所以用仓库里这份翻译
Name: "chinesesimp"; MessagesFile: "ChineseSimplified.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "..\dist\KeySound\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs; Excludes: "*.pdb,*.map"

[Icons]
Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; WorkingDir: "{app}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; WorkingDir: "{app}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
; 配置和运行时目录是程序自己建的，安装包没带。空的就清掉，里面有用户音效就留着
Type: files; Name: "{app}\config.json"
Type: filesandordirs; Name: "{app}\tmp"
Type: dirifempty; Name: "{app}\sounds"
