// COM 外壳：类厂 + 自注册。
// 注意这个 DLL 绝对不能带嵌入式 manifest，带了会在 audiodg 的保护环境里触发
// 不允许的调用，表现是加载失败、前几秒没声音、之后彻底忽略这个 APO。
// 链接选项里的 /MANIFEST:NO 就是为了这个（实现方案.md 第 3 节第 4 条）。
#include <windows.h>
#include <objbase.h>
#include <new>

#include "KeySoundApo.h"

static LONG g_module_refs = 0;
static HMODULE g_module = NULL;

void ModuleAddRef() {
    InterlockedIncrement(&g_module_refs);
}

void ModuleRelease() {
    InterlockedDecrement(&g_module_refs);
}

class CKeySoundApoFactory : public IClassFactory {
public:
    CKeySoundApoFactory() : ref_(1) { ModuleAddRef(); }

    STDMETHOD(QueryInterface)(REFIID riid, void** ppv) {
        if (ppv == NULL) {
            return E_POINTER;
        }
        KeySoundTraceGuid(riid == __uuidof(IClassFactory) || riid == __uuidof(IUnknown) ? 10 : 11, riid);
        if (riid == __uuidof(IClassFactory) || riid == __uuidof(IUnknown)) {
            *ppv = static_cast<IClassFactory*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = NULL;
        return E_NOINTERFACE;
    }

    STDMETHOD_(ULONG, AddRef)() {
        return (ULONG)InterlockedIncrement(&ref_);
    }

    STDMETHOD_(ULONG, Release)() {
        const LONG left = InterlockedDecrement(&ref_);
        if (left == 0) {
            delete this;
        }
        return (ULONG)left;
    }

    STDMETHOD(CreateInstance)(IUnknown* outer, REFIID riid, void** ppv) {
        if (ppv == NULL) {
            return E_POINTER;
        }
        *ppv = NULL;
        KeySoundTrace(19, NULL, outer != NULL ? 1 : 0);
        KeySoundTraceGuid(20, riid);
        // 聚合时 COM 规定只能先要 IUnknown，别的接口由外层再来查
        if (outer != NULL && riid != __uuidof(IUnknown)) {
            return CLASS_E_NOAGGREGATION;
        }
        CKeySoundApo* apo = new (std::nothrow) CKeySoundApo(outer);
        if (apo == NULL) {
            return E_OUTOFMEMORY;
        }
        const HRESULT hr = apo->InternalQueryInterface(riid, ppv);
        apo->InternalRelease();
        return hr;
    }

    STDMETHOD(LockServer)(BOOL lock) {
        if (lock) {
            ModuleAddRef();
        } else {
            ModuleRelease();
        }
        return S_OK;
    }

private:
    ~CKeySoundApoFactory() { ModuleRelease(); }

    LONG ref_;
};

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void** ppv) {
    if (ppv == NULL) {
        return E_POINTER;
    }
    *ppv = NULL;
    KeySoundTraceGuid(1, rclsid);
    KeySoundTraceGuid(2, riid);
    if (rclsid != CLSID_KeySoundApo) {
        return CLASS_E_CLASSNOTAVAILABLE;
    }
    CKeySoundApoFactory* factory = new (std::nothrow) CKeySoundApoFactory();
    if (factory == NULL) {
        return E_OUTOFMEMORY;
    }
    const HRESULT hr = factory->QueryInterface(riid, ppv);
    factory->Release();
    return hr;
}

STDAPI DllCanUnloadNow() {
    return (g_module_refs == 0) ? S_OK : S_FALSE;
}

// regsvr32 用。正常安装走 vmic_setup.exe，它会连端点的 FxProperties 一起写
STDAPI DllRegisterServer() {
    WCHAR path[MAX_PATH] = {};
    if (GetModuleFileNameW(g_module, path, ARRAYSIZE(path)) == 0) {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    HKEY clsid_key = NULL;
    LONG rc = RegCreateKeyExW(HKEY_LOCAL_MACHINE,
                              L"SOFTWARE\\Classes\\CLSID\\" KEYSOUND_APO_CLSID_STRING,
                              0, NULL, 0, KEY_WRITE | KEY_WOW64_64KEY, NULL, &clsid_key, NULL);
    if (rc != ERROR_SUCCESS) {
        return HRESULT_FROM_WIN32(rc);
    }
    const WCHAR* name = KEYSOUND_APO_FRIENDLY_NAME;
    RegSetValueExW(clsid_key, NULL, 0, REG_SZ, reinterpret_cast<const BYTE*>(name),
                   (DWORD)((wcslen(name) + 1) * sizeof(WCHAR)));

    HKEY inproc_key = NULL;
    rc = RegCreateKeyExW(clsid_key, L"InprocServer32", 0, NULL, 0,
                         KEY_WRITE | KEY_WOW64_64KEY, NULL, &inproc_key, NULL);
    if (rc == ERROR_SUCCESS) {
        RegSetValueExW(inproc_key, NULL, 0, REG_SZ, reinterpret_cast<const BYTE*>(path),
                       (DWORD)((wcslen(path) + 1) * sizeof(WCHAR)));
        RegSetValueExW(inproc_key, L"ThreadingModel", 0, REG_SZ,
                       reinterpret_cast<const BYTE*>(L"Both"), (DWORD)(5 * sizeof(WCHAR)));
        RegCloseKey(inproc_key);
    }
    RegCloseKey(clsid_key);
    return HRESULT_FROM_WIN32(rc);
}

STDAPI DllUnregisterServer() {
    RegDeleteKeyExW(HKEY_LOCAL_MACHINE,
                    L"SOFTWARE\\Classes\\CLSID\\" KEYSOUND_APO_CLSID_STRING L"\\InprocServer32",
                    KEY_WOW64_64KEY, 0);
    RegDeleteKeyExW(HKEY_LOCAL_MACHINE,
                    L"SOFTWARE\\Classes\\CLSID\\" KEYSOUND_APO_CLSID_STRING,
                    KEY_WOW64_64KEY, 0);
    return S_OK;
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved) {
    UNREFERENCED_PARAMETER(reserved);
    if (reason == DLL_PROCESS_ATTACH) {
        g_module = module;
        DisableThreadLibraryCalls(module);
    }
    return TRUE;
}
