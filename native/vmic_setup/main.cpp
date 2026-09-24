// vmic_setup.exe：把 KeySound 的 APO 挂到某只麦克风端点上，或者干净地摘下来。
// 需要管理员权限，由 KeySound 用 ShellExecuteW(L"runas") 拉起，弹一次 UAC。
//
// 用法：
//   vmic_setup.exe install   --dll <绝对路径> --endpoint <端点GUID> [--slot efx|mfx|sfx]
//                            [--backup <目录>] [--result <文件>] [--modes] [--keep-protected]
//   vmic_setup.exe uninstall --endpoint <端点GUID> [--backup <目录>] [--result <文件>]
//   vmic_setup.exe status    --endpoint <端点GUID> [--result <文件>]
//   vmic_setup.exe testtone  on|off
//
// 结果写进 --result 指定的 UTF-8 JSON 文件：提权进程的 stdout 传不回普通权限的父进程，
// 只能落盘再让 Python 去读。
#include <windows.h>
#include <objbase.h>
#include <shlobj.h>
#include <mmreg.h>
#include <ks.h>
#include <ksmedia.h>
#include <fcntl.h>
#include <io.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include <string>
#include <vector>

#include "../common/KeySoundShared.h"

// ---------------------------------------------------------------- 常量

static const wchar_t* kCaptureRoot =
    L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\MMDevices\\Audio\\Capture";
static const wchar_t* kAudioPolicyKey =
    L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Audio";
static const wchar_t* kDisableProtectedValue = L"DisableProtectedAudioDG";

// 属性名就是「属性集GUID,属性ID」这种字符串，都是 REG_SZ（实现方案.md 第 5 节）
static const wchar_t* kFxSfx = L"{d04e05a6-594b-4fb6-a80d-01af5eed7d1d},5";
static const wchar_t* kFxMfx = L"{d04e05a6-594b-4fb6-a80d-01af5eed7d1d},6";
static const wchar_t* kFxEfx = L"{d04e05a6-594b-4fb6-a80d-01af5eed7d1d},7";
static const wchar_t* kFxLfx = L"{d04e05a6-594b-4fb6-a80d-01af5eed7d1d},1";
static const wchar_t* kFxGfx = L"{d04e05a6-594b-4fb6-a80d-01af5eed7d1d},2";
// Windows 10 1809 之后，端点上只要有这组「复合效果」列表，音频引擎就只看它，
// 上面那三个单 CLSID 会被忽略。这台机器的 Realtek 麦克风就是这种
static const wchar_t* kCompSfx = L"{d04e05a6-594b-4fb6-a80d-01af5eed7d1d},13";
static const wchar_t* kCompMfx = L"{d04e05a6-594b-4fb6-a80d-01af5eed7d1d},14";
static const wchar_t* kCompEfx = L"{d04e05a6-594b-4fb6-a80d-01af5eed7d1d},15";
static const wchar_t* kModesSfx = L"{d3993a3f-99c2-4402-b5ec-a92a0367664b},5";
static const wchar_t* kModesMfx = L"{d3993a3f-99c2-4402-b5ec-a92a0367664b},6";
static const wchar_t* kModesEfx = L"{d3993a3f-99c2-4402-b5ec-a92a0367664b},7";
// IAudioSystemEffects，和 KeySoundApo::GetRegistrationProperties 里报的那个一致
static const wchar_t* kApoInterface = L"{5FA00F27-ADD6-499A-8A9D-6B98521FA75B}";
static const wchar_t* kApoClassRoot =
    L"SOFTWARE\\Classes\\AudioEngine\\AudioProcessingObjects\\";
// 原来的 SFX CLSID 记在这里，APO 初始化时把它当成子效果先跑一遍
static const wchar_t* kChildApoRoot = L"SOFTWARE\\KeySound\\Child APOs\\";
// 这个是 1 的话所有 sAPO 都不加载（声音设置里「启用音频增强」那个勾）
static const wchar_t* kDisableSysFx = L"{1da5d803-d492-4edd-8c23-e0c0ffee7f0e},5";
// 读设备名用
static const wchar_t* kPropDeviceDesc = L"{a45c254e-df1c-4efd-8020-67d146a850e0},2";
static const wchar_t* kPropFriendlyName = L"{a45c254e-df1c-4efd-8020-67d146a850e0},14";

// 备份要覆盖的范围：五个槽位 + 三个处理模式属性，一个都不能少。
// 只删掉自己写的值是不够的——OEM（Realtek / Nahimic）原本就可能占着同一个槽位，
// 不按备份恢复会让用户的音效增强直接消失
static const wchar_t* kFxBackupValues[] = {
    kFxSfx, kFxMfx, kFxEfx, kFxLfx, kFxGfx,
    kCompSfx, kCompMfx, kCompEfx,
    kModesSfx, kModesMfx, kModesEfx,
};

struct SlotInfo {
    const wchar_t* name;
    const wchar_t* fx_value;
    const wchar_t* composite_value;
    const wchar_t* modes_value;
};

static const SlotInfo kSlots[] = {
    { L"efx", kFxEfx, kCompEfx, kModesEfx },
    { L"mfx", kFxMfx, kCompMfx, kModesMfx },
    { L"sfx", kFxSfx, kCompSfx, kModesSfx },
};

// ---------------------------------------------------------------- 小工具

static void Log(const wchar_t* format, ...) {
    va_list args;
    va_start(args, format);
    vfwprintf(stdout, format, args);
    va_end(args);
    fputws(L"\n", stdout);
}

static std::wstring JsonEscape(const std::wstring& text) {
    std::wstring out;
    for (size_t i = 0; i < text.size(); ++i) {
        const wchar_t c = text[i];
        if (c == L'"' || c == L'\\') {
            out += L'\\';
            out += c;
        } else if (c == L'\n') {
            out += L"\\n";
        } else if (c == L'\r') {
            out += L"\\r";
        } else if (c == L'\t') {
            out += L"\\t";
        } else {
            out += c;
        }
    }
    return out;
}

static void WriteResultFile(const std::wstring& path, bool ok, const std::wstring& message,
                            const std::wstring& extra) {
    if (path.empty()) {
        return;
    }
    std::wstring json = L"{\"ok\": ";
    json += ok ? L"true" : L"false";
    json += L", \"message\": \"" + JsonEscape(message) + L"\"";
    if (!extra.empty()) {
        json += L", " + extra;
    }
    json += L"}";

    FILE* file = NULL;
    if (_wfopen_s(&file, path.c_str(), L"w, ccs=UTF-8") != 0 || file == NULL) {
        return;
    }
    fputws(json.c_str(), file);
    fclose(file);
}

static bool EnablePrivilege(const wchar_t* name) {
    HANDLE token = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token)) {
        return false;
    }
    TOKEN_PRIVILEGES tp = {};
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    bool ok = LookupPrivilegeValueW(NULL, name, &tp.Privileges[0].Luid) != 0;
    if (ok) {
        ok = AdjustTokenPrivileges(token, FALSE, &tp, sizeof(tp), NULL, NULL) != 0 &&
             GetLastError() == ERROR_SUCCESS;
    }
    CloseHandle(token);
    return ok;
}

// MMDevices 下的端点键默认只给管理员读权限，直接写会 ERROR_ACCESS_DENIED。
// 先拿 SeBackupPrivilege / SeRestorePrivilege，再用 REG_OPTION_BACKUP_RESTORE 打开，
// 这样绕过 DACL 检查，也就不用去改系统 ACL 或者夺取所有权那么脏的做法
static LONG OpenKeyRW(HKEY root, const std::wstring& path, bool create, HKEY* out) {
    const REGSAM sam = KEY_READ | KEY_WRITE | KEY_WOW64_64KEY;
    DWORD disposition = 0;
    LONG rc = RegCreateKeyExW(root, path.c_str(), 0, NULL, REG_OPTION_BACKUP_RESTORE,
                              sam, NULL, out, &disposition);
    if (rc == ERROR_SUCCESS) {
        if (!create && disposition == REG_CREATED_NEW_KEY) {
            // 只是想读，别留下空键
        }
        return rc;
    }
    return RegCreateKeyExW(root, path.c_str(), 0, NULL, 0, sam, NULL, out, &disposition);
}

static LONG OpenKeyRead(HKEY root, const std::wstring& path, HKEY* out) {
    const REGSAM sam = KEY_READ | KEY_WOW64_64KEY;
    LONG rc = RegOpenKeyExW(root, path.c_str(), REG_OPTION_BACKUP_RESTORE, sam, out);
    if (rc == ERROR_SUCCESS) {
        return rc;
    }
    return RegOpenKeyExW(root, path.c_str(), 0, sam, out);
}

static bool ReadValueRaw(HKEY key, const wchar_t* name, DWORD* type, std::vector<BYTE>* data) {
    DWORD size = 0;
    LONG rc = RegQueryValueExW(key, name, NULL, type, NULL, &size);
    if (rc != ERROR_SUCCESS) {
        return false;
    }
    data->resize(size);
    if (size == 0) {
        return true;
    }
    rc = RegQueryValueExW(key, name, NULL, type, &(*data)[0], &size);
    if (rc != ERROR_SUCCESS) {
        return false;
    }
    data->resize(size);
    return true;
}

static std::wstring ReadStringValue(HKEY key, const wchar_t* name) {
    DWORD type = 0;
    std::vector<BYTE> data;
    if (!ReadValueRaw(key, name, &type, &data) || type != REG_SZ || data.size() < sizeof(wchar_t)) {
        return L"";
    }
    const wchar_t* text = reinterpret_cast<const wchar_t*>(&data[0]);
    const size_t chars = data.size() / sizeof(wchar_t);
    size_t length = 0;
    while (length < chars && text[length] != L'\0') {
        ++length;
    }
    return std::wstring(text, length);
}

static LONG WriteStringValue(HKEY key, const wchar_t* name, const wchar_t* value) {
    return RegSetValueExW(key, name, 0, REG_SZ, reinterpret_cast<const BYTE*>(value),
                          (DWORD)((wcslen(value) + 1) * sizeof(wchar_t)));
}

static LONG WriteDwordValue(HKEY key, const wchar_t* name, DWORD value) {
    return RegSetValueExW(key, name, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&value),
                          sizeof(value));
}

static bool SameText(const std::wstring& left, const std::wstring& right) {
    return _wcsicmp(left.c_str(), right.c_str()) == 0;
}

// REG_MULTI_SZ 拆成字符串。旧的单 CLSID 有时是 REG_SZ，也认
static std::vector<std::wstring> ReadMultiSz(HKEY key, const wchar_t* name) {
    std::vector<std::wstring> items;
    DWORD type = 0;
    std::vector<BYTE> data;
    if (!ReadValueRaw(key, name, &type, &data) || data.size() < sizeof(wchar_t)) {
        return items;
    }
    const wchar_t* text = reinterpret_cast<const wchar_t*>(&data[0]);
    const size_t chars = data.size() / sizeof(wchar_t);
    if (type == REG_SZ) {
        size_t length = 0;
        while (length < chars && text[length] != L'\0') {
            ++length;
        }
        if (length > 0) {
            items.push_back(std::wstring(text, length));
        }
        return items;
    }
    if (type != REG_MULTI_SZ) {
        return items;
    }
    size_t index = 0;
    while (index < chars && text[index] != L'\0') {
        const size_t start = index;
        while (index < chars && text[index] != L'\0') {
            ++index;
        }
        if (index > start) {
            items.push_back(std::wstring(text + start, index - start));
        }
        ++index;
    }
    return items;
}

static LONG WriteMultiSz(HKEY key, const wchar_t* name, const std::vector<std::wstring>& items) {
    std::wstring multi;
    for (size_t i = 0; i < items.size(); ++i) {
        multi += items[i];
        multi += L'\0';
    }
    multi += L'\0';
    return RegSetValueExW(key, name, 0, REG_MULTI_SZ,
                          reinterpret_cast<const BYTE*>(multi.c_str()),
                          (DWORD)(multi.size() * sizeof(wchar_t)));
}

static void AppendGuid(HKEY key, const wchar_t* name, const wchar_t* guid) {
    std::vector<std::wstring> items = ReadMultiSz(key, name);
    for (size_t i = 0; i < items.size(); ++i) {
        if (SameText(items[i], guid)) {
            return;
        }
    }
    items.push_back(guid);
    WriteMultiSz(key, name, items);
}

static void RemoveGuid(HKEY key, const wchar_t* name, const wchar_t* guid) {
    std::vector<std::wstring> items = ReadMultiSz(key, name);
    std::vector<std::wstring> kept;
    bool changed = false;
    for (size_t i = 0; i < items.size(); ++i) {
        if (SameText(items[i], guid)) {
            changed = true;
            continue;
        }
        kept.push_back(items[i]);
    }
    if (!changed) {
        return;
    }
    if (kept.empty()) {
        RegDeleteValueW(key, name);
        return;
    }
    WriteMultiSz(key, name, kept);
}

static std::wstring ToHex(const std::vector<BYTE>& data) {
    static const wchar_t* digits = L"0123456789abcdef";
    std::wstring out;
    out.reserve(data.size() * 2);
    for (size_t i = 0; i < data.size(); ++i) {
        out += digits[data[i] >> 4];
        out += digits[data[i] & 0x0f];
    }
    return out;
}

static std::vector<BYTE> FromHex(const std::wstring& text) {
    std::vector<BYTE> out;
    for (size_t i = 0; i + 1 < text.size(); i += 2) {
        const wchar_t pair[3] = { text[i], text[i + 1], 0 };
        out.push_back((BYTE)wcstoul(pair, NULL, 16));
    }
    return out;
}

// ---------------------------------------------------------------- 备份 / 恢复

// 一行一个值：<F|P>|<值名>|<注册表类型，-1 表示原来就没有>|<十六进制的原始字节>
struct BackupEntry {
    wchar_t scope;
    std::wstring name;
    long type;
    std::vector<BYTE> data;
};

static std::wstring BackupPath(const std::wstring& dir, const std::wstring& endpoint,
                               const wchar_t* suffix) {
    std::wstring name = endpoint;
    // 端点 GUID 带花括号，文件名里能用，但去掉更省心
    std::wstring clean;
    for (size_t i = 0; i < name.size(); ++i) {
        if (name[i] != L'{' && name[i] != L'}') {
            clean += name[i];
        }
    }
    return dir + L"\\" + clean + suffix;
}

static void CollectBackup(HKEY fx_key, HKEY props_key, std::vector<BackupEntry>* entries) {
    for (size_t i = 0; i < ARRAYSIZE(kFxBackupValues); ++i) {
        BackupEntry entry;
        entry.scope = L'F';
        entry.name = kFxBackupValues[i];
        DWORD type = 0;
        if (fx_key != NULL && ReadValueRaw(fx_key, kFxBackupValues[i], &type, &entry.data)) {
            entry.type = (long)type;
        } else {
            entry.type = -1;
        }
        entries->push_back(entry);
    }
    BackupEntry sysfx;
    sysfx.scope = L'P';
    sysfx.name = kDisableSysFx;
    DWORD type = 0;
    if (props_key != NULL && ReadValueRaw(props_key, kDisableSysFx, &type, &sysfx.data)) {
        sysfx.type = (long)type;
    } else {
        sysfx.type = -1;
    }
    entries->push_back(sysfx);
}

static bool SaveBackup(const std::wstring& path, const std::vector<BackupEntry>& entries) {
    FILE* file = NULL;
    if (_wfopen_s(&file, path.c_str(), L"w, ccs=UTF-8") != 0 || file == NULL) {
        return false;
    }
    fputws(L"KEYSOUND-VMIC-BACKUP 1\n", file);
    for (size_t i = 0; i < entries.size(); ++i) {
        const BackupEntry& entry = entries[i];
        fwprintf(file, L"%c|%s|%ld|%s\n", entry.scope, entry.name.c_str(), entry.type,
                 ToHex(entry.data).c_str());
    }
    fclose(file);
    return true;
}

static bool LoadBackup(const std::wstring& path, std::vector<BackupEntry>* entries) {
    FILE* file = NULL;
    if (_wfopen_s(&file, path.c_str(), L"r, ccs=UTF-8") != 0 || file == NULL) {
        return false;
    }
    wchar_t line[8192];
    bool first = true;
    while (fgetws(line, ARRAYSIZE(line), file) != NULL) {
        std::wstring text(line);
        while (!text.empty() && (text[text.size() - 1] == L'\n' || text[text.size() - 1] == L'\r')) {
            text.erase(text.size() - 1);
        }
        if (first) {
            first = false;
            continue;
        }
        if (text.empty()) {
            continue;
        }
        size_t p1 = text.find(L'|');
        size_t p2 = (p1 == std::wstring::npos) ? p1 : text.find(L'|', p1 + 1);
        size_t p3 = (p2 == std::wstring::npos) ? p2 : text.find(L'|', p2 + 1);
        if (p3 == std::wstring::npos) {
            continue;
        }
        BackupEntry entry;
        entry.scope = text[0];
        entry.name = text.substr(p1 + 1, p2 - p1 - 1);
        entry.type = wcstol(text.substr(p2 + 1, p3 - p2 - 1).c_str(), NULL, 10);
        entry.data = FromHex(text.substr(p3 + 1));
        entries->push_back(entry);
    }
    fclose(file);
    return true;
}

// 用 reg.exe 再导一份整键，纯粹是给人工救砖用的，失败了不影响流程
static void ExportRescueReg(const std::wstring& endpoint, const std::wstring& path) {
    std::wstring command = L"reg.exe export \"HKLM\\";
    command += kCaptureRoot;
    command += L"\\" + endpoint + L"\" \"" + path + L"\" /y";

    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi = {};
    std::vector<wchar_t> buffer(command.begin(), command.end());
    buffer.push_back(L'\0');
    if (CreateProcessW(NULL, &buffer[0], NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL,
                       &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 15000);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
}

// ---------------------------------------------------------------- 服务

static bool WaitForServiceState(SC_HANDLE service, DWORD wanted, DWORD timeout_ms) {
    const DWORD deadline = GetTickCount() + timeout_ms;
    SERVICE_STATUS_PROCESS status = {};
    DWORD needed = 0;
    while (GetTickCount() < deadline) {
        if (!QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO,
                                  reinterpret_cast<BYTE*>(&status), sizeof(status), &needed)) {
            return false;
        }
        if (status.dwCurrentState == wanted) {
            return true;
        }
        Sleep(200);
    }
    return false;
}

static void StopServiceTree(SC_HANDLE scm, const wchar_t* name,
                            std::vector<std::wstring>* stopped) {
    SC_HANDLE service = OpenServiceW(scm, name,
                                     SERVICE_STOP | SERVICE_QUERY_STATUS | SERVICE_ENUMERATE_DEPENDENTS);
    if (service == NULL) {
        return;
    }

    DWORD bytes = 0;
    DWORD count = 0;
    EnumDependentServicesW(service, SERVICE_ACTIVE, NULL, 0, &bytes, &count);
    if (bytes > 0) {
        std::vector<BYTE> buffer(bytes);
        LPENUM_SERVICE_STATUSW list = reinterpret_cast<LPENUM_SERVICE_STATUSW>(&buffer[0]);
        if (EnumDependentServicesW(service, SERVICE_ACTIVE, list, bytes, &bytes, &count)) {
            for (DWORD i = 0; i < count; ++i) {
                StopServiceTree(scm, list[i].lpServiceName, stopped);
            }
        }
    }

    SERVICE_STATUS status = {};
    if (ControlService(service, SERVICE_CONTROL_STOP, &status)) {
        WaitForServiceState(service, SERVICE_STOPPED, 15000);
        stopped->push_back(name);
    } else if (GetLastError() == ERROR_SERVICE_NOT_ACTIVE) {
        stopped->push_back(name);
    }
    CloseServiceHandle(service);
}

static bool StartOneService(SC_HANDLE scm, const wchar_t* name) {
    SC_HANDLE service = OpenServiceW(scm, name, SERVICE_START | SERVICE_QUERY_STATUS);
    if (service == NULL) {
        return false;
    }
    bool ok = StartServiceW(service, 0, NULL) != 0;
    if (!ok && GetLastError() == ERROR_SERVICE_ALREADY_RUNNING) {
        ok = true;
    }
    if (ok) {
        WaitForServiceState(service, SERVICE_RUNNING, 20000);
    }
    CloseServiceHandle(service);
    return ok;
}

// 改完 FxProperties 要让音频栈重新读一遍。重启 AudioEndpointBuilder 会顺带把
// 依赖它的 audiosrv 一起停掉，起回来时端点属性就是新的了。
// 代价是全系统音频会中断两三秒，界面上要提示用户
static bool RestartAudioStack() {
    SC_HANDLE scm = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT | SC_MANAGER_ENUMERATE_SERVICE);
    if (scm == NULL) {
        return false;
    }
    std::vector<std::wstring> stopped;
    StopServiceTree(scm, L"AudioEndpointBuilder", &stopped);
    bool ok = true;
    // stopped 里是「依赖方在前、被依赖方在后」，起回来要反过来
    for (size_t i = stopped.size(); i > 0; --i) {
        if (!StartOneService(scm, stopped[i - 1].c_str())) {
            ok = false;
        }
    }
    CloseServiceHandle(scm);
    return ok;
}

// ---------------------------------------------------------------- 命令

static const SlotInfo* FindSlot(const std::wstring& name) {
    for (size_t i = 0; i < ARRAYSIZE(kSlots); ++i) {
        if (_wcsicmp(kSlots[i].name, name.c_str()) == 0) {
            return &kSlots[i];
        }
    }
    return NULL;
}

static LONG RegisterCom(const std::wstring& dll_path) {
    HKEY clsid_key = NULL;
    LONG rc = OpenKeyRW(HKEY_LOCAL_MACHINE,
                        std::wstring(L"SOFTWARE\\Classes\\CLSID\\") + KEYSOUND_APO_CLSID_STRING,
                        true, &clsid_key);
    if (rc != ERROR_SUCCESS) {
        return rc;
    }
    WriteStringValue(clsid_key, NULL, KEYSOUND_APO_FRIENDLY_NAME);

    HKEY inproc_key = NULL;
    rc = OpenKeyRW(clsid_key, L"InprocServer32", true, &inproc_key);
    if (rc == ERROR_SUCCESS) {
        // 写绝对路径，不学 Soundpad 往 system32 里塞文件，省得被杀软盯上
        rc = WriteStringValue(inproc_key, NULL, dll_path.c_str());
        WriteStringValue(inproc_key, L"ThreadingModel", L"Both");
        RegCloseKey(inproc_key);
    }
    RegCloseKey(clsid_key);
    if (rc != ERROR_SUCCESS) {
        return rc;
    }

    // 复合效果列表里的 CLSID，音频引擎会先到这里核对，没有这项就直接跳过
    HKEY apo_key = NULL;
    rc = OpenKeyRW(HKEY_LOCAL_MACHINE,
                   std::wstring(kApoClassRoot) + KEYSOUND_APO_CLSID_STRING,
                   true, &apo_key);
    if (rc != ERROR_SUCCESS) {
        return rc;
    }
    WriteStringValue(apo_key, L"FriendlyName", KEYSOUND_APO_FRIENDLY_NAME);
    WriteStringValue(apo_key, L"Copyright", KEYSOUND_APO_COPYRIGHT);
    WriteDwordValue(apo_key, L"MajorVersion", 1);
    WriteDwordValue(apo_key, L"MinorVersion", 0);
    // 必须和 GetRegistrationProperties 一致：就地 + 采样率一致 + 容器位深一致。
    // 0x1 | 0x4 | 0x8。引擎靠这个把 SFX 前面的数据转成 32 位浮点
    WriteDwordValue(apo_key, L"Flags", 0x1 | 0x4 | 0x8);
    WriteDwordValue(apo_key, L"MinInputConnections", 1);
    WriteDwordValue(apo_key, L"MaxInputConnections", 1);
    WriteDwordValue(apo_key, L"MinOutputConnections", 1);
    WriteDwordValue(apo_key, L"MaxOutputConnections", 1);
    WriteDwordValue(apo_key, L"MaxInstances", 0xFFFFFFFF);
    WriteDwordValue(apo_key, L"NumAPOInterfaces", 1);
    WriteStringValue(apo_key, L"APOInterface0", kApoInterface);
    RegCloseKey(apo_key);
    return ERROR_SUCCESS;
}

static void UnregisterCom() {
    const std::wstring base = std::wstring(L"SOFTWARE\\Classes\\CLSID\\") + KEYSOUND_APO_CLSID_STRING;
    RegDeleteKeyExW(HKEY_LOCAL_MACHINE, (base + L"\\InprocServer32").c_str(), KEY_WOW64_64KEY, 0);
    RegDeleteKeyExW(HKEY_LOCAL_MACHINE, base.c_str(), KEY_WOW64_64KEY, 0);
    RegDeleteKeyExW(HKEY_LOCAL_MACHINE,
                    (std::wstring(kApoClassRoot) + KEYSOUND_APO_CLSID_STRING).c_str(),
                    KEY_WOW64_64KEY, 0);
}

static void SetProtectedAudioDG(bool disable_protection) {
    HKEY key = NULL;
    if (OpenKeyRW(HKEY_LOCAL_MACHINE, kAudioPolicyKey, true, &key) != ERROR_SUCCESS) {
        return;
    }
    if (disable_protection) {
        // 没给 DLL 买签名证书之前只能这样，代价是受 DRM 保护的音频播不了。
        // 长期解法是买个普通 Authenticode 证书给 DLL 签名
        WriteDwordValue(key, kDisableProtectedValue, 1);
    } else {
        RegDeleteValueW(key, kDisableProtectedValue);
    }
    RegCloseKey(key);
}

static void RememberChild(const std::wstring& endpoint, const std::wstring& child_guid) {
    HKEY key = NULL;
    if (OpenKeyRW(HKEY_LOCAL_MACHINE, std::wstring(kChildApoRoot) + endpoint, true, &key) != ERROR_SUCCESS) {
        return;
    }
    WriteStringValue(key, L"PreMixChild", child_guid.c_str());
    RegCloseKey(key);
}

static void ForgetChild(const std::wstring& endpoint) {
    const std::wstring path = std::wstring(kChildApoRoot) + endpoint;
    RegDeleteKeyExW(HKEY_LOCAL_MACHINE, path.c_str(), KEY_WOW64_64KEY, 0);
    RegDeleteKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\KeySound\\Child APOs", KEY_WOW64_64KEY, 0);
}

static void WriteModesProperty(HKEY fx_key, const wchar_t* value_name) {
    // 模式 GUID 一律从 Windows SDK 的 ksmedia.h 取，别用记忆里的字面值
    static const GUID modes[] = {
        { STATIC_AUDIO_SIGNALPROCESSINGMODE_DEFAULT },
        { STATIC_AUDIO_SIGNALPROCESSINGMODE_RAW },
        { STATIC_AUDIO_SIGNALPROCESSINGMODE_COMMUNICATIONS },
    };
    std::wstring multi;
    for (size_t i = 0; i < ARRAYSIZE(modes); ++i) {
        wchar_t text[64] = {};
        if (StringFromGUID2(modes[i], text, ARRAYSIZE(text)) > 0) {
            multi += text;
            multi += L'\0';
        }
    }
    multi += L'\0';
    RegSetValueExW(fx_key, value_name, 0, REG_MULTI_SZ,
                   reinterpret_cast<const BYTE*>(multi.c_str()),
                   (DWORD)(multi.size() * sizeof(wchar_t)));
}

// audiodg 拒绝加载用户目录里的 DLL。智能应用控制还会按哈希拦住没签名的新文件，
// 所以正式挂上的那份必须在 Program Files，并且用本机「KeySound Local APO」证书签过
static std::wstring FindSignTool() {
    std::wstring best;
    WIN32_FIND_DATAW data = {};
    HANDLE find = FindFirstFileW(L"C:\\Program Files (x86)\\Windows Kits\\10\\bin\\*", &data);
    if (find == INVALID_HANDLE_VALUE) {
        return best;
    }
    do {
        if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 || data.cFileName[0] == L'.') {
            continue;
        }
        std::wstring candidate = L"C:\\Program Files (x86)\\Windows Kits\\10\\bin\\";
        candidate += data.cFileName;
        candidate += L"\\x64\\signtool.exe";
        if (GetFileAttributesW(candidate.c_str()) != INVALID_FILE_ATTRIBUTES && candidate > best) {
            best = candidate;
        }
    } while (FindNextFileW(find, &data));
    FindClose(find);
    return best;
}

static bool RunHidden(const std::wstring& command) {
    std::vector<wchar_t> buffer(command.begin(), command.end());
    buffer.push_back(0);
    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi = {};
    if (!CreateProcessW(NULL, &buffer[0], NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        return false;
    }
    WaitForSingleObject(pi.hProcess, 60000);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return code == 0;
}

static void SignStagedDll(const std::wstring& dll) {
    const std::wstring tool = FindSignTool();
    if (tool.empty()) {
        Log(L"没找到 signtool，跳过签名");
        return;
    }
    const std::wstring command = L"\"" + tool + L"\" sign /sm /fd SHA256 /n \"KeySound Local APO\" \"" + dll + L"\"";
    if (!RunHidden(command)) {
        Log(L"签名失败。若智能应用控制开着，audiodg 会拒绝加载这份 DLL");
    }
}

static std::wstring StageDll(const std::wstring& source) {
    wchar_t program_files[MAX_PATH] = {};
    if (SHGetFolderPathW(NULL, CSIDL_PROGRAM_FILES, NULL, SHGFP_TYPE_CURRENT, program_files) != S_OK) {
        return L"";
    }
    const std::wstring dir = std::wstring(program_files) + L"\\KeySound";
    CreateDirectoryW(dir.c_str(), NULL);
    const std::wstring dest = dir + L"\\KeySoundApo.dll";
    bool copied = (_wcsicmp(source.c_str(), dest.c_str()) == 0);
    if (!copied) {
        copied = CopyFileW(source.c_str(), dest.c_str(), FALSE) != 0;
        if (!copied) {
            // 正在采集时 audiodg 占着旧文件，先把音频栈停掉再覆盖
            RestartAudioStack();
            copied = CopyFileW(source.c_str(), dest.c_str(), FALSE) != 0;
        }
    }
    if (!copied) {
        return L"";
    }
    SignStagedDll(dest);
    return dest;
}

static int CommandInstall(const std::wstring& dll_path, const std::wstring& endpoint,
                          const std::wstring& slot_name, const std::wstring& backup_dir,
                          const std::wstring& result_path, bool write_modes,
                          bool keep_protected) {
    const SlotInfo* slot = FindSlot(slot_name);
    if (slot == NULL) {
        Log(L"槽位只能是 efx / mfx / sfx，收到的是：%s", slot_name.c_str());
        WriteResultFile(result_path, false, L"槽位参数不对，只能是 efx / mfx / sfx", L"");
        return 2;
    }
    if (GetFileAttributesW(dll_path.c_str()) == INVALID_FILE_ATTRIBUTES) {
        Log(L"找不到 DLL：%s", dll_path.c_str());
        WriteResultFile(result_path, false, L"找不到 KeySoundApo.dll，先把它放到 KeySound 目录里", L"");
        return 2;
    }

    HKEY fx_key = NULL;
    HKEY props_key = NULL;
    const std::wstring endpoint_path = std::wstring(kCaptureRoot) + L"\\" + endpoint;
    LONG rc = OpenKeyRW(HKEY_LOCAL_MACHINE, endpoint_path + L"\\FxProperties", true, &fx_key);
    if (rc != ERROR_SUCCESS) {
        Log(L"打不开端点的 FxProperties，错误码 %ld", rc);
        WriteResultFile(result_path, false, L"打不开这只麦克风的注册表项，确认是以管理员身份运行的", L"");
        return 3;
    }
    OpenKeyRW(HKEY_LOCAL_MACHINE, endpoint_path + L"\\Properties", false, &props_key);

    // 备份必须在写之前，而且已经备份过就不能再覆盖，
    // 否则第二次 install 会把我们自己写进去的值当成「原始值」记下来
    SHCreateDirectoryExW(NULL, backup_dir.c_str(), NULL);
    const std::wstring backup_file = BackupPath(backup_dir, endpoint, L".backup");
    if (GetFileAttributesW(backup_file.c_str()) == INVALID_FILE_ATTRIBUTES) {
        std::vector<BackupEntry> entries;
        CollectBackup(fx_key, props_key, &entries);
        if (!SaveBackup(backup_file, entries)) {
            Log(L"备份写不进去：%s", backup_file.c_str());
            WriteResultFile(result_path, false, L"注册表备份写不进去，为了能干净恢复，这里直接中止", L"");
            RegCloseKey(fx_key);
            if (props_key) {
                RegCloseKey(props_key);
            }
            return 4;
        }
        ExportRescueReg(endpoint, BackupPath(backup_dir, endpoint, L".reg"));
    }

    const std::wstring staged = StageDll(dll_path);
    if (staged.empty()) {
        Log(L"复制 DLL 到 Program Files 失败");
        WriteResultFile(result_path, false, L"没法把 KeySoundApo.dll 装到 Program Files", L"");
        RegCloseKey(fx_key);
        if (props_key) {
            RegCloseKey(props_key);
        }
        return 4;
    }
    rc = RegisterCom(staged);
    if (rc != ERROR_SUCCESS) {
        Log(L"注册 COM 失败，错误码 %ld", rc);
        WriteResultFile(result_path, false, L"注册 COM 组件失败", L"");
        RegCloseKey(fx_key);
        if (props_key) {
            RegCloseKey(props_key);
        }
        return 5;
    }

    // 麦克风只挂 SFX，和 Equalizer APO 对采集设备的做法一样。
    // EFX 紧挨硬件，格式经常是 16 位 PCM；把我们追加进那条复合链后，
    // 录音机拿到的是静音，而系统设置里的电平条还在动（它看的是效果处理前的电平）。
    UNREFERENCED_PARAMETER(slot);
    UNREFERENCED_PARAMETER(write_modes);
    std::wstring child;
    const std::vector<std::wstring> composite = ReadMultiSz(fx_key, kCompSfx);
    for (size_t i = 0; i < composite.size(); ++i) {
        if (!composite[i].empty() && !SameText(composite[i], KEYSOUND_APO_CLSID_STRING)) {
            child = composite[i];
            break;
        }
    }
    if (child.empty()) {
        const std::wstring single = ReadStringValue(fx_key, kFxSfx);
        if (!single.empty() && !SameText(single, KEYSOUND_APO_CLSID_STRING)) {
            child = single;
        }
    }
    RememberChild(endpoint, child);

    rc = WriteStringValue(fx_key, kFxSfx, KEYSOUND_APO_CLSID_STRING);
    if (rc != ERROR_SUCCESS) {
        Log(L"写 SFX 失败，错误码 %ld", rc);
        WriteResultFile(result_path, false, L"往麦克风端点写 APO 配置失败", L"");
        RegCloseKey(fx_key);
        if (props_key) {
            RegCloseKey(props_key);
        }
        return 6;
    }
    if (!composite.empty()) {
        // 复合列表存在时引擎不看上面的单 CLSID。列表里只留我们，
        // 原来的 CLSID 已经记成子 APO，处理时先调用它，不再并排挂第二个
        std::vector<std::wstring> only_us;
        only_us.push_back(KEYSOUND_APO_CLSID_STRING);
        WriteMultiSz(fx_key, kCompSfx, only_us);
    }
    if (ReadMultiSz(fx_key, kModesSfx).empty()) {
        // 缺省模式就是 DEFAULT。不要把 RAW 写进厂商的 EFX，那会改变原来的采集路径
        std::vector<std::wstring> modes;
        modes.push_back(L"{C18E2F7E-933D-4965-B7D1-1EEF228D2AF3}");
        WriteMultiSz(fx_key, kModesSfx, modes);
    }

    if (props_key != NULL) {
        DWORD type = 0;
        std::vector<BYTE> data;
        if (ReadValueRaw(props_key, kDisableSysFx, &type, &data) && type == REG_DWORD &&
            data.size() >= sizeof(DWORD) && *reinterpret_cast<DWORD*>(&data[0]) != 0) {
            WriteDwordValue(props_key, kDisableSysFx, 0);
        }
    }

    if (!keep_protected) {
        SetProtectedAudioDG(true);
    }

    RegCloseKey(fx_key);
    if (props_key != NULL) {
        RegCloseKey(props_key);
    }

    Log(L"配置写好了，正在重启音频服务…");
    const bool restarted = RestartAudioStack();
    Log(restarted ? L"音频服务已重启" : L"音频服务重启失败，可能需要手动重启系统");

    std::wstring extra = L"\"restarted\": ";
    extra += restarted ? L"true" : L"false";
    extra += L", \"slot\": \"sfx\"";
    WriteResultFile(result_path, true,
                    restarted ? L"已挂到这只麦克风上" : L"配置已写入，但音频服务没重启成功，重启一次系统再试",
                    extra);
    return 0;
}

static int CommandUninstall(const std::wstring& endpoint, const std::wstring& backup_dir,
                            const std::wstring& result_path) {
    const std::wstring endpoint_path = std::wstring(kCaptureRoot) + L"\\" + endpoint;
    HKEY fx_key = NULL;
    HKEY props_key = NULL;
    OpenKeyRW(HKEY_LOCAL_MACHINE, endpoint_path + L"\\FxProperties", false, &fx_key);
    OpenKeyRW(HKEY_LOCAL_MACHINE, endpoint_path + L"\\Properties", false, &props_key);

    const std::wstring backup_file = BackupPath(backup_dir, endpoint, L".backup");
    std::vector<BackupEntry> entries;
    if (LoadBackup(backup_file, &entries) && !entries.empty()) {
        for (size_t i = 0; i < entries.size(); ++i) {
            const BackupEntry& entry = entries[i];
            HKEY key = (entry.scope == L'P') ? props_key : fx_key;
            if (key == NULL) {
                continue;
            }
            if (entry.type < 0) {
                RegDeleteValueW(key, entry.name.c_str());
            } else {
                RegSetValueExW(key, entry.name.c_str(), 0, (DWORD)entry.type,
                               entry.data.empty() ? NULL : &entry.data[0],
                               (DWORD)entry.data.size());
            }
        }
        DeleteFileW(backup_file.c_str());
    } else if (fx_key != NULL) {
        // 没有备份（比如用户手动删过）只能退而求其次：把值是我们 CLSID 的槽位删掉，
        // 别人的值一律不动
        for (size_t i = 0; i < ARRAYSIZE(kFxBackupValues); ++i) {
            if (_wcsicmp(ReadStringValue(fx_key, kFxBackupValues[i]).c_str(),
                         KEYSOUND_APO_CLSID_STRING) == 0) {
                RegDeleteValueW(fx_key, kFxBackupValues[i]);
            }
        }
    }

    // 第一次安装时的备份还没有复合列表。恢复完再从列表里摘掉我们，避免 Realtek 的项被一起清掉
    if (fx_key != NULL) {
        RemoveGuid(fx_key, kCompSfx, KEYSOUND_APO_CLSID_STRING);
        RemoveGuid(fx_key, kCompMfx, KEYSOUND_APO_CLSID_STRING);
        RemoveGuid(fx_key, kCompEfx, KEYSOUND_APO_CLSID_STRING);
    }

    if (fx_key != NULL) {
        RegCloseKey(fx_key);
    }
    if (props_key != NULL) {
        RegCloseKey(props_key);
    }

    ForgetChild(endpoint);
    UnregisterCom();
    SetProtectedAudioDG(false);

    Log(L"已恢复，正在重启音频服务…");
    const bool restarted = RestartAudioStack();
    std::wstring extra = L"\"restarted\": ";
    extra += restarted ? L"true" : L"false";
    WriteResultFile(result_path, true,
                    restarted ? L"已经从这只麦克风上摘下来了" : L"注册表已恢复，但音频服务没重启成功",
                    extra);
    return 0;
}

static int CommandStatus(const std::wstring& endpoint, const std::wstring& result_path) {
    const std::wstring endpoint_path = std::wstring(kCaptureRoot) + L"\\" + endpoint;
    HKEY fx_key = NULL;
    std::wstring installed_slot;
    if (OpenKeyRead(HKEY_LOCAL_MACHINE, endpoint_path + L"\\FxProperties", &fx_key) == ERROR_SUCCESS) {
        for (size_t i = 0; i < ARRAYSIZE(kSlots); ++i) {
            if (_wcsicmp(ReadStringValue(fx_key, kSlots[i].fx_value).c_str(),
                         KEYSOUND_APO_CLSID_STRING) == 0) {
                installed_slot = kSlots[i].name;
                break;
            }
        }
        RegCloseKey(fx_key);
    }

    HKEY props_key = NULL;
    std::wstring name;
    if (OpenKeyRead(HKEY_LOCAL_MACHINE, endpoint_path + L"\\Properties", &props_key) == ERROR_SUCCESS) {
        name = ReadStringValue(props_key, kPropFriendlyName);
        if (name.empty()) {
            name = ReadStringValue(props_key, kPropDeviceDesc);
        }
        RegCloseKey(props_key);
    }

    Log(L"设备：%s", name.c_str());
    Log(L"槽位：%s", installed_slot.empty() ? L"（没挂）" : installed_slot.c_str());

    std::wstring extra = L"\"slot\": \"" + JsonEscape(installed_slot) + L"\"";
    extra += L", \"name\": \"" + JsonEscape(name) + L"\"";
    WriteResultFile(result_path, true, L"", extra);
    return 0;
}

// Phase 0 用：让 APO 无条件注入 440Hz 正弦波，不需要 KeySound 参与，
// 用来单独验证「我们的 APO 到底能不能让 Discord 听见」
static int CommandTestTone(bool on, const std::wstring& result_path) {
    HKEY key = NULL;
    if (OpenKeyRW(HKEY_LOCAL_MACHINE, KEYSOUND_SETTINGS_KEY, true, &key) != ERROR_SUCCESS) {
        WriteResultFile(result_path, false, L"写不了 HKLM\\SOFTWARE\\KeySound", L"");
        return 3;
    }
    WriteDwordValue(key, KEYSOUND_TESTTONE_VALUE, on ? 1 : 0);
    RegCloseKey(key);
    Log(L"测试音已%s，重启音频服务后生效", on ? L"打开" : L"关闭");
    RestartAudioStack();
    WriteResultFile(result_path, true, on ? L"测试音已打开" : L"测试音已关闭", L"");
    return 0;
}

// ---------------------------------------------------------------- 入口

static std::wstring ArgValue(int argc, wchar_t** argv, const wchar_t* name,
                             const wchar_t* fallback) {
    for (int i = 1; i + 1 < argc; ++i) {
        if (_wcsicmp(argv[i], name) == 0) {
            return argv[i + 1];
        }
    }
    return fallback;
}

static bool HasFlag(int argc, wchar_t** argv, const wchar_t* name) {
    for (int i = 1; i < argc; ++i) {
        if (_wcsicmp(argv[i], name) == 0) {
            return true;
        }
    }
    return false;
}

static std::wstring DefaultBackupDir() {
    wchar_t* appdata = NULL;
    size_t length = 0;
    if (_wdupenv_s(&appdata, &length, L"APPDATA") == 0 && appdata != NULL) {
        std::wstring dir = std::wstring(appdata) + L"\\KeySound\\vmic-backup";
        free(appdata);
        return dir;
    }
    return L"C:\\ProgramData\\KeySound\\vmic-backup";
}

int wmain(int argc, wchar_t** argv) {
    _setmode(_fileno(stdout), _O_U16TEXT);

    if (argc < 2) {
        Log(L"用法：vmic_setup.exe install|uninstall|status|testtone …");
        return 1;
    }

    // 有这两个特权才能绕过 MMDevices 那些只读的 ACL
    EnablePrivilege(SE_BACKUP_NAME);
    EnablePrivilege(SE_RESTORE_NAME);

    const std::wstring command = argv[1];
    const std::wstring result_path = ArgValue(argc, argv, L"--result", L"");
    const std::wstring endpoint = ArgValue(argc, argv, L"--endpoint", L"");
    const std::wstring backup_dir = ArgValue(argc, argv, L"--backup", DefaultBackupDir().c_str());

    if (_wcsicmp(command.c_str(), L"testtone") == 0) {
        const bool on = (argc >= 3) && _wcsicmp(argv[2], L"on") == 0;
        return CommandTestTone(on, result_path);
    }

    if (endpoint.empty()) {
        Log(L"缺 --endpoint");
        WriteResultFile(result_path, false, L"没有指定麦克风端点", L"");
        return 1;
    }

    if (_wcsicmp(command.c_str(), L"install") == 0) {
        return CommandInstall(ArgValue(argc, argv, L"--dll", L""), endpoint,
                              ArgValue(argc, argv, L"--slot", L"efx"), backup_dir, result_path,
                              HasFlag(argc, argv, L"--modes"),
                              HasFlag(argc, argv, L"--keep-protected"));
    }
    if (_wcsicmp(command.c_str(), L"uninstall") == 0) {
        return CommandUninstall(endpoint, backup_dir, result_path);
    }
    if (_wcsicmp(command.c_str(), L"status") == 0) {
        return CommandStatus(endpoint, result_path);
    }

    Log(L"不认识的命令：%s", command.c_str());
    WriteResultFile(result_path, false, L"不认识的命令", L"");
    return 1;
}
