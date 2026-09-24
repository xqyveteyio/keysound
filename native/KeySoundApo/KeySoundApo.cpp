#include "KeySoundApo.h"
#include "MediaType.h"

#include <propsys.h>
#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <wchar.h>
#include <new>

// dllmain.cpp 里的模块引用计数，DllCanUnloadNow 要用
void ModuleAddRef();
void ModuleRelease();

static const float kTestToneHz = 440.0f;
static const float kTestToneGain = 0.25f;
static const float kTwoPi = 6.283185307179586f;

// 句柄故意不关：图建到一半失败时 DLL 会被卸掉，命名段还得留着给外面读。
// stage: 1 类厂请求的 CLSID，2 类厂请求的 IID，10/11 类厂 QueryInterface 成功/失败，
// 20 CreateInstance，30/31 对象 QueryInterface 成功/失败，3 Initialize，4 协商，5 Lock
struct KeySoundTraceEvent {
    UINT32 stage;
    UINT32 a;
    UINT32 b;
    UINT32 c;
    UINT32 d;
};

struct KeySoundTraceHeader {
    UINT32 magic;
    UINT32 count;
    KeySoundTraceEvent events[64];
};

static KeySoundTraceHeader* TraceView() {
    static KeySoundTraceHeader* view = NULL;
    if (view != NULL) {
        return view;
    }
    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = FALSE;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
            KEYSOUND_RING_SDDL, SDDL_REVISION_1, &sa.lpSecurityDescriptor, NULL)) {
        sa.lpSecurityDescriptor = NULL;
    }
    HANDLE mapping = CreateFileMappingW(INVALID_HANDLE_VALUE,
                                        sa.lpSecurityDescriptor ? &sa : NULL,
                                        PAGE_READWRITE, 0, sizeof(KeySoundTraceHeader),
                                        L"Global\\KeySoundApoTrace");
    const DWORD create_error = GetLastError();
    if (sa.lpSecurityDescriptor) {
        LocalFree(sa.lpSecurityDescriptor);
    }
    if (mapping == NULL) {
        return NULL;
    }
    view = static_cast<KeySoundTraceHeader*>(
        MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, 0));
    if (view == NULL) {
        return NULL;
    }
    if (create_error != ERROR_ALREADY_EXISTS) {
        ZeroMemory(view, sizeof(KeySoundTraceHeader));
    }
    view->magic = 0x4B535452;
    return view;
}

static void TracePush(UINT32 stage, UINT32 a, UINT32 b, UINT32 c, UINT32 d) {
    KeySoundTraceHeader* view = TraceView();
    if (view == NULL) {
        return;
    }
    const UINT32 index = view->count % 64;
    view->events[index].stage = stage;
    view->events[index].a = a;
    view->events[index].b = b;
    view->events[index].c = c;
    view->events[index].d = d;
    view->count += 1;
    view->magic = 0x4B535452;
}

void KeySoundTrace(UINT32 stage, const WAVEFORMATEX* format, UINT32 kind) {
    if (format == NULL) {
        TracePush(stage, 0, 0, 0, kind);
        return;
    }
    TracePush(stage, format->wFormatTag,
              format->wBitsPerSample | ((UINT32)format->nChannels << 16),
              format->nSamplesPerSec, kind);
}

void KeySoundTraceGuid(UINT32 stage, REFGUID guid) {
    UINT32 data4 = 0;
    memcpy(&data4, guid.Data4, sizeof(data4));
    TracePush(stage, guid.Data1, (UINT32)guid.Data2 | ((UINT32)guid.Data3 << 16), data4, 0);
}

// 协商和 LockForProcess 都在非实时线程，可以落盘。APOProcess 里不能调用这个。
// audiodg 跑在 LOCAL SERVICE 下，写当前用户的 Temp 会失败，所以放 Windows\Temp
static void ApoLog(const char* format, ...) {
    static volatile LONG lines = 0;
    if (InterlockedIncrement(&lines) > 40) {
        return;
    }
    FILE* file = NULL;
    if (fopen_s(&file, "C:\\Windows\\Temp\\keysound-apo.log", "a") != 0 || file == NULL) {
        return;
    }
    va_list args;
    va_start(args, format);
    vfprintf(file, format, args);
    va_end(args);
    fputc('\n', file);
    fclose(file);
}

CKeySoundApo::CKeySoundApo(IUnknown* outer)
    : inner_(this),
      outer_(NULL),
      free_marshaler_(NULL),
      ref_(1),
      initialized_(false),
      locked_(false),
      sample_rate_(0),
      channels_(0),
      negotiated_channels_(2),
      max_frames_(0),
      sample_kind_(kSamplePassthrough),
      bytes_per_frame_(0),
      mix_buffer_(NULL),
      child_apo_(NULL),
      child_rt_(NULL),
      child_cfg_(NULL),
      test_tone_(false),
      tone_phase_(0.0f) {
    // 没人聚合时，委托目标就是自己的内部 IUnknown，引用计数不会跑到外面去
    outer_ = (outer != NULL) ? outer : static_cast<IUnknown*>(&inner_);
    ModuleAddRef();
    // 不增加我们自己的引用，避免和封送器互相卡住释放不掉
    CoCreateFreeThreadedMarshaler(static_cast<IUnknown*>(&inner_), &free_marshaler_);
}

CKeySoundApo::~CKeySoundApo() {
    UnlockForProcess();
    ResetChild();
    if (free_marshaler_ != NULL) {
        free_marshaler_->Release();
        free_marshaler_ = NULL;
    }
    ModuleRelease();
}

STDMETHODIMP CKeySoundApo::QueryInterface(REFIID riid, void** ppv) {
    return outer_->QueryInterface(riid, ppv);
}

STDMETHODIMP_(ULONG) CKeySoundApo::AddRef() {
    return outer_->AddRef();
}

STDMETHODIMP_(ULONG) CKeySoundApo::Release() {
    return outer_->Release();
}

HRESULT CKeySoundApo::InternalQueryInterface(REFIID riid, void** ppv) {
    if (ppv == NULL) {
        return E_POINTER;
    }
    if (free_marshaler_ != NULL &&
        (riid == __uuidof(IMarshal) || riid == kIidAgileObject)) {
        return free_marshaler_->QueryInterface(riid, ppv);
    }
    IUnknown* iface = NULL;
    if (riid == __uuidof(IUnknown)) {
        iface = static_cast<IUnknown*>(&inner_);
    } else if (riid == __uuidof(IAudioProcessingObject)) {
        iface = static_cast<IAudioProcessingObject*>(this);
    } else if (riid == __uuidof(IAudioProcessingObjectRT)) {
        iface = static_cast<IAudioProcessingObjectRT*>(this);
    } else if (riid == __uuidof(IAudioProcessingObjectConfiguration)) {
        iface = static_cast<IAudioProcessingObjectConfiguration*>(this);
    } else if (riid == __uuidof(IAudioSystemEffects2)) {
        iface = static_cast<IAudioSystemEffects2*>(this);
    } else if (riid == __uuidof(IAudioSystemEffects)) {
        iface = static_cast<IAudioSystemEffects*>(this);
    } else if (riid == kIidAgileObject) {
        // 和内部 IUnknown 是同一个指针：这是个空标记接口，没有自己的方法
        iface = static_cast<IUnknown*>(&inner_);
    } else {
        *ppv = NULL;
        KeySoundTraceGuid(31, riid);
        return E_NOINTERFACE;
    }
    KeySoundTraceGuid(30, riid);
    // 走这份接口自己的 AddRef。聚合成员的 AddRef 会记到外层，不能一律改内部计数
    iface->AddRef();
    *ppv = iface;
    return S_OK;
}

ULONG CKeySoundApo::InternalAddRef() {
    return (ULONG)InterlockedIncrement(&ref_);
}

ULONG CKeySoundApo::InternalRelease() {
    const LONG left = InterlockedDecrement(&ref_);
    if (left == 0) {
        delete this;
    }
    return (ULONG)left;
}

STDMETHODIMP CKeySoundApo::Reset() {
    tone_phase_ = 0.0f;
    KeySoundTrace(11, NULL, 0);
    return S_OK;
}

STDMETHODIMP CKeySoundApo::GetLatency(HNSTIME* time) {
    if (time == NULL) {
        return E_POINTER;
    }
    // 就地相加，不引入额外延迟
    *time = 0;
    KeySoundTrace(12, NULL, 0);
    return S_OK;
}

STDMETHODIMP CKeySoundApo::GetRegistrationProperties(APO_REG_PROPERTIES** props) {
    if (props == NULL) {
        return E_POINTER;
    }
    APO_REG_PROPERTIES* out =
        static_cast<APO_REG_PROPERTIES*>(CoTaskMemAlloc(sizeof(APO_REG_PROPERTIES)));
    if (out == NULL) {
        return E_OUTOFMEMORY;
    }
    ZeroMemory(out, sizeof(APO_REG_PROPERTIES));
    out->clsid = CLSID_KeySoundApo;
    // 和 Equalizer APO 一样：采样率、容器位深必须一致，并且就地处理。
    // 声道数不要求一致。注册表 AudioEngine\AudioProcessingObjects 里的 Flags 必须是同一个值，
    // 否则引擎核对完就丢掉这个 APO。
    out->Flags = (APO_FLAG)(APO_FLAG_INPLACE | APO_FLAG_FRAMESPERSECOND_MUST_MATCH |
                            APO_FLAG_BITSPERSAMPLE_MUST_MATCH);
    wcscpy_s(out->szFriendlyName, ARRAYSIZE(out->szFriendlyName), KEYSOUND_APO_FRIENDLY_NAME);
    wcscpy_s(out->szCopyrightInfo, ARRAYSIZE(out->szCopyrightInfo), KEYSOUND_APO_COPYRIGHT);
    out->u32MajorVersion = 1;
    out->u32MinorVersion = 0;
    out->u32MinInputConnections = 1;
    out->u32MaxInputConnections = 1;
    out->u32MinOutputConnections = 1;
    out->u32MaxOutputConnections = 1;
    out->u32MaxInstances = INFINITE;
    out->u32NumAPOInterfaces = 1;
    out->iidAPOInterfaceList[0] = __uuidof(IAudioSystemEffects);
    *props = out;
    KeySoundTrace(13, NULL, out->Flags);
    return S_OK;
}

void CKeySoundApo::ResetChild() {
    if (child_apo_ != NULL) {
        child_apo_->Release();
        child_apo_ = NULL;
    }
    if (child_rt_ != NULL) {
        child_rt_->Release();
        child_rt_ = NULL;
    }
    if (child_cfg_ != NULL) {
        child_cfg_->Release();
        child_cfg_ = NULL;
    }
}

// 安装器把原来的 SFX CLSID 记在这个键里。没有就直接透传，不要因此让麦克风打不开
void CKeySoundApo::LoadChild(UINT32 data_size, BYTE* data) {
    ResetChild();
    // 跟 APOInitSystemEffects 的前两项对齐。后面字段各版本长度不一样，只读端点属性
    struct InitView {
        APOInitBaseStruct APOInit;
        IPropertyStore* endpoint;
    };
    if (data == NULL || data_size < sizeof(InitView)) {
        return;
    }
    IPropertyStore* endpoint = reinterpret_cast<InitView*>(data)->endpoint;
    if (endpoint == NULL) {
        return;
    }

    // PKEY_AudioEndpoint_GUID {1da5d803-d492-4edd-8c23-e0c0ffee7f0e},4
    PROPERTYKEY endpoint_key = {};
    endpoint_key.fmtid.Data1 = 0x1da5d803;
    endpoint_key.fmtid.Data2 = 0xd492;
    endpoint_key.fmtid.Data3 = 0x4edd;
    endpoint_key.fmtid.Data4[0] = 0x8c;
    endpoint_key.fmtid.Data4[1] = 0x23;
    endpoint_key.fmtid.Data4[2] = 0xe0;
    endpoint_key.fmtid.Data4[3] = 0xc0;
    endpoint_key.fmtid.Data4[4] = 0xff;
    endpoint_key.fmtid.Data4[5] = 0xee;
    endpoint_key.fmtid.Data4[6] = 0x7f;
    endpoint_key.fmtid.Data4[7] = 0x0e;
    endpoint_key.pid = 4;

    PROPVARIANT var;
    PropVariantInit(&var);
    if (FAILED(endpoint->GetValue(endpoint_key, &var)) || var.vt != VT_LPWSTR || var.pwszVal == NULL) {
        PropVariantClear(&var);
        return;
    }

    WCHAR key_path[256] = L"SOFTWARE\\KeySound\\Child APOs\\";
    wcscat_s(key_path, var.pwszVal);
    PropVariantClear(&var);

    WCHAR child_guid[80] = {};
    HKEY key = NULL;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, key_path, 0,
                      KEY_QUERY_VALUE | KEY_WOW64_64KEY, &key) != ERROR_SUCCESS) {
        return;
    }
    DWORD size = sizeof(child_guid);
    DWORD type = 0;
    const LONG rc = RegQueryValueExW(key, L"PreMixChild", NULL, &type,
                                     reinterpret_cast<BYTE*>(child_guid), &size);
    RegCloseKey(key);
    if (rc != ERROR_SUCCESS || type != REG_SZ || child_guid[0] == L'\0') {
        return;
    }
    if (_wcsicmp(child_guid, KEYSOUND_APO_CLSID_STRING) == 0) {
        return;
    }

    GUID clsid = {};
    if (FAILED(CLSIDFromString(child_guid, &clsid))) {
        return;
    }
    // 子 APO 不要聚合进来，它是独立对象，失败就丢掉，麦克风仍由我们透传
    if (FAILED(CoCreateInstance(clsid, NULL, CLSCTX_INPROC_SERVER,
                                __uuidof(IAudioProcessingObject),
                                reinterpret_cast<void**>(&child_apo_)))) {
        ResetChild();
        return;
    }
    if (FAILED(child_apo_->QueryInterface(__uuidof(IAudioProcessingObjectRT),
                                          reinterpret_cast<void**>(&child_rt_))) ||
        FAILED(child_apo_->QueryInterface(__uuidof(IAudioProcessingObjectConfiguration),
                                          reinterpret_cast<void**>(&child_cfg_))) ||
        FAILED(child_apo_->Initialize(data_size, data))) {
        ApoLog("child apo init failed");
        ResetChild();
        return;
    }
    ApoLog("child apo ready");
}

STDMETHODIMP CKeySoundApo::Initialize(UINT32 data_size, BYTE* data) {
    // 子 APO 创建失败不能让整次 Initialize 失败，否则这只麦克风会没声音
    initialized_ = true;
    LoadChild(data_size, data);
    KeySoundTrace(3, NULL, child_apo_ != NULL ? 1 : 0);
    return S_OK;
}

// SFX 槽位在厂商效果之后。这里只接受 32 位浮点，采样率跟对侧一致。
// 对不上就交回一份浮点格式（S_FALSE），让引擎自己做转换，而不是硬吃 PCM 把采样解释错
HRESULT CKeySoundApo::NegotiateFormat(IAudioMediaType* opposite, IAudioMediaType* requested,
                                        IAudioMediaType** supported) {
    if (supported == NULL) {
        return E_POINTER;
    }
    *supported = NULL;
    if (requested == NULL) {
        return E_POINTER;
    }
    UNCOMPRESSEDAUDIOFORMAT req = {};
    if (FAILED(requested->GetUncompressedAudioFormat(&req)) || req.dwSamplesPerFrame == 0 ||
        req.fFramesPerSecond <= 0.0f) {
        return E_INVALIDARG;
    }

    UNCOMPRESSEDAUDIOFORMAT other = {};
    const bool have_other = opposite != NULL &&
                            SUCCEEDED(opposite->GetUncompressedAudioFormat(&other)) &&
                            other.fFramesPerSecond > 0.0f;
    const float rate = have_other ? other.fFramesPerSecond : req.fFramesPerSecond;
    const bool rate_ok = !have_other || (req.fFramesPerSecond > rate - 0.5f &&
                                         req.fFramesPerSecond < rate + 0.5f);
    const bool is_float = IsEqualGUID(req.guidFormatType, kSubtypeIeeeFloat) &&
                          req.dwBytesPerSampleContainer == 4 && req.dwValidBitsPerSample == 32;
    if (is_float && rate_ok && req.dwSamplesPerFrame <= 8) {
        negotiated_channels_ = req.dwSamplesPerFrame;
        CApoMediaType* created = CApoMediaType::Create(requested->GetAudioFormat());
        if (created == NULL) {
            return E_OUTOFMEMORY;
        }
        *supported = created;
        KeySoundTrace(4, requested->GetAudioFormat(), (UINT32)kSampleFloat32);
        return S_OK;
    }

    WORD channels = (WORD)req.dwSamplesPerFrame;
    if (channels == 0 || channels > 8) {
        channels = 2;
    }
    DWORD mask = req.dwChannelMask;
    if (mask == 0 && channels == 2) {
        mask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
    } else if (mask == 0 && channels == 1) {
        mask = SPEAKER_FRONT_CENTER;
    }
    CApoMediaType* created = CApoMediaType::CreateFloat32((DWORD)rate, channels, mask);
    if (created == NULL) {
        return E_OUTOFMEMORY;
    }
    *supported = created;
    negotiated_channels_ = channels;
    KeySoundTrace(4, created->GetAudioFormat(), (UINT32)kSampleFloat32);
    ApoLog("negotiate rewrite to float ch=%u rate=%lu", channels, (DWORD)rate);
    return S_FALSE;
}

STDMETHODIMP CKeySoundApo::IsInputFormatSupported(IAudioMediaType* opposite,
                                                  IAudioMediaType* requested,
                                                  IAudioMediaType** supported) {
    return NegotiateFormat(opposite, requested, supported);
}

STDMETHODIMP CKeySoundApo::IsOutputFormatSupported(IAudioMediaType* opposite,
                                                   IAudioMediaType* requested,
                                                   IAudioMediaType** supported) {
    return NegotiateFormat(opposite, requested, supported);
}

STDMETHODIMP CKeySoundApo::GetEffectsList(LPGUID* effects, UINT* count, HANDLE event) {
    UNREFERENCED_PARAMETER(event);
    if (effects == NULL || count == NULL) {
        return E_POINTER;
    }
    // 不向系统暴露可开关的音效项，只负责把共享内存里的声音混进去
    *effects = NULL;
    *count = 0;
    KeySoundTrace(14, NULL, 0);
    return S_OK;
}

STDMETHODIMP CKeySoundApo::GetInputChannelCount(UINT32* channel_count) {
    if (channel_count == NULL) {
        return E_POINTER;
    }
    // 锁之前 channels_ 还是 0。直接回 0 的话引擎认为这条链路没有声道，采集图会拆掉
    *channel_count = (channels_ != 0) ? channels_ : negotiated_channels_;
    KeySoundTrace(6, NULL, *channel_count);
    return S_OK;
}

bool CKeySoundApo::ReadTestToneFlag() {
    HKEY key = NULL;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, KEYSOUND_SETTINGS_KEY, 0,
                      KEY_QUERY_VALUE | KEY_WOW64_64KEY, &key) != ERROR_SUCCESS) {
        return false;
    }
    DWORD value = 0;
    DWORD size = sizeof(value);
    DWORD type = 0;
    const LONG rc = RegQueryValueExW(key, KEYSOUND_TESTTONE_VALUE, NULL, &type,
                                     reinterpret_cast<BYTE*>(&value), &size);
    RegCloseKey(key);
    return rc == ERROR_SUCCESS && type == REG_DWORD && value != 0;
}

STDMETHODIMP CKeySoundApo::LockForProcess(UINT32 num_input_connections,
                                          APO_CONNECTION_DESCRIPTOR** input_connections,
                                          UINT32 num_output_connections,
                                          APO_CONNECTION_DESCRIPTOR** output_connections) {
    if (num_input_connections != 1 || num_output_connections != 1 ||
        input_connections == NULL || output_connections == NULL ||
        input_connections[0] == NULL || output_connections[0] == NULL) {
        return E_INVALIDARG;
    }
    if (locked_) {
        UnlockForProcess();
    }

    IAudioMediaType* type = input_connections[0]->pFormat;
    if (type == NULL) {
        return E_INVALIDARG;
    }
    const WAVEFORMATEX* format = type->GetAudioFormat();
    if (format == NULL || format->nChannels == 0 || format->nSamplesPerSec == 0) {
        return E_INVALIDARG;
    }

    // 格式对不上就透传，不要返回 APOERR_FORMAT_NOT_SUPPORTED：
    // 那个错误会让 Windows 把这只麦克风整条链路标成打不开
    sample_kind_ = ClassifyFormat(format);
    sample_rate_ = format->nSamplesPerSec;
    channels_ = format->nChannels;
    bytes_per_frame_ = format->nBlockAlign;
    if (bytes_per_frame_ == 0) {
        bytes_per_frame_ = channels_ * ((UINT32)format->wBitsPerSample / 8);
    }
    UINT32 expected = 0;
    if (sample_kind_ == kSampleFloat32 || sample_kind_ == kSamplePcm32 ||
        sample_kind_ == kSamplePcm24In32) {
        expected = channels_ * 4;
    } else if (sample_kind_ == kSamplePcm16) {
        expected = channels_ * 2;
    } else if (sample_kind_ == kSamplePcm24Packed) {
        expected = channels_ * 3;
    }
    if (expected != 0 && bytes_per_frame_ != expected) {
        sample_kind_ = kSamplePassthrough;
    }

    max_frames_ = input_connections[0]->u32MaxFrameCount;
    if (max_frames_ == 0) {
        max_frames_ = sample_rate_ / 10;  // 兜底 100ms，别让缓冲变成 0
    }

    if (sample_kind_ != kSamplePassthrough) {
        mix_buffer_ = new (std::nothrow) float[(size_t)max_frames_ * channels_];
        if (mix_buffer_ == NULL) {
            return E_OUTOFMEMORY;
        }
    }

    // 这里是非实时线程，读注册表、建共享内存都只能放在这一步；
    // 也只有到了这里格式才最终定下来，头部才填得出采样率和声道数
    test_tone_ = ReadTestToneFlag();
    const bool ring_ok = ring_.Create(sample_rate_, channels_);

    if (child_cfg_ != NULL) {
        const HRESULT child_hr = child_cfg_->LockForProcess(
            num_input_connections, input_connections, num_output_connections, output_connections);
        if (FAILED(child_hr)) {
            // 子 APO 锁不上就丢掉，只透传。不能把失败返回给引擎，否则整只麦克风打不开
            ApoLog("child lock failed %08lx", (unsigned long)child_hr);
            ResetChild();
        }
    }

    tone_phase_ = 0.0f;
    locked_ = true;
    KeySoundTrace(5, format, sample_kind_);
    ApoLog("lock kind=%u rate=%lu ch=%u align=%u frames=%u ring=%d",
           sample_kind_, sample_rate_, channels_, bytes_per_frame_, max_frames_,
           ring_ok ? 1 : 0);
    return S_OK;
}

STDMETHODIMP CKeySoundApo::UnlockForProcess() {
    if (child_cfg_ != NULL) {
        child_cfg_->UnlockForProcess();
    }
    locked_ = false;
    ring_.Close();
    delete[] mix_buffer_;
    mix_buffer_ = NULL;
    return S_OK;
}

STDMETHODIMP_(UINT32) CKeySoundApo::CalcInputFrames(UINT32 output_frame_count) {
    return output_frame_count;
}

STDMETHODIMP_(UINT32) CKeySoundApo::CalcOutputFrames(UINT32 input_frame_count) {
    return input_frame_count;
}

static float ClampUnit(float value) {
    if (value > 1.0f) {
        return 1.0f;
    }
    if (value < -1.0f) {
        return -1.0f;
    }
    return value;
}

// 实时回调。这里面不能加锁、不能分配释放内存、不能碰注册表和文件、不能打日志，
// 任何一样都会表现成「没声音」或者 audiodg 崩掉（实现方案.md 7.1 硬性约束）
STDMETHODIMP_(void) CKeySoundApo::APOProcess(UINT32 num_input_connections,
                                             APO_CONNECTION_PROPERTY** input_connections,
                                             UINT32 num_output_connections,
                                             APO_CONNECTION_PROPERTY** output_connections) {
    if (num_input_connections == 0 || num_output_connections == 0 ||
        input_connections == NULL || output_connections == NULL) {
        return;
    }
    APO_CONNECTION_PROPERTY* in = input_connections[0];
    APO_CONNECTION_PROPERTY* out = output_connections[0];
    if (in == NULL || out == NULL || !locked_) {
        return;
    }

    ring_.Heartbeat();

    const UINT32 frames = in->u32ValidFrameCount;
    out->u32ValidFrameCount = frames;
    const APO_BUFFER_FLAGS in_flags = (APO_BUFFER_FLAGS)in->u32BufferFlags;
    if ((in_flags != BUFFER_VALID && in_flags != BUFFER_SILENT) || frames == 0 ||
        in->pBuffer == 0 || out->pBuffer == 0 || bytes_per_frame_ == 0) {
        out->u32BufferFlags = in_flags;
        return;
    }

    BYTE* input_bytes = reinterpret_cast<BYTE*>(in->pBuffer);
    BYTE* output_bytes = reinterpret_cast<BYTE*>(out->pBuffer);
    const size_t bytes = (size_t)frames * bytes_per_frame_;
    // 静音标志表示缓冲内容未定义。清的是输入，不是输出：就地处理时两者是同一块，
    // 但有效语音（BUFFER_VALID）绝不能在这里被清掉
    if (in_flags == BUFFER_SILENT) {
        memset(input_bytes, 0, bytes);
    }

    if (child_rt_ != NULL) {
        child_rt_->APOProcess(num_input_connections, input_connections,
                              num_output_connections, output_connections);
    } else if (output_bytes != input_bytes) {
        memcpy(output_bytes, input_bytes, bytes);
    }

    bool added = false;
    if (sample_kind_ == kSampleFloat32 && mix_buffer_ != NULL && channels_ != 0) {
        UINT32 todo = frames;
        if (todo > max_frames_) {
            todo = max_frames_;
        }
        UINT32 mixed = 0;
        if (test_tone_) {
            const float step = kTwoPi * kTestToneHz / (float)sample_rate_;
            float phase = tone_phase_;
            for (UINT32 f = 0; f < todo; ++f) {
                const float value = sinf(phase) * kTestToneGain;
                phase += step;
                if (phase > kTwoPi) {
                    phase -= kTwoPi;
                }
                for (UINT32 c = 0; c < channels_; ++c) {
                    mix_buffer_[(size_t)f * channels_ + c] = value;
                }
            }
            tone_phase_ = phase;
            mixed = todo;
        } else {
            mixed = ring_.Read(mix_buffer_, todo);
        }
        if (mixed > 0) {
            float* dst = reinterpret_cast<float*>(output_bytes);
            const size_t count = (size_t)mixed * channels_;
            for (size_t i = 0; i < count; ++i) {
                dst[i] = ClampUnit(dst[i] + mix_buffer_[i]);
            }
            added = true;
        }
    }

    if (in_flags == BUFFER_VALID) {
        out->u32BufferFlags = BUFFER_VALID;
        return;
    }
    // 进来是静音：没叠上音效就保持 SILENT。叠上了再改成 VALID，
    // 这样按键声在没人说话时也能出去，同时又不会把驱动依赖的静音标志冲掉
    if (!added || sample_kind_ != kSampleFloat32) {
        out->u32BufferFlags = BUFFER_SILENT;
        return;
    }
    float* dst = reinterpret_cast<float*>(output_bytes);
    const size_t count = (size_t)frames * channels_;
    bool silent = true;
    for (size_t i = 0; i < count; ++i) {
        if (dst[i] > 1.0e-7f || dst[i] < -1.0e-7f) {
            silent = false;
            break;
        }
    }
    out->u32BufferFlags = silent ? BUFFER_SILENT : BUFFER_VALID;
}
