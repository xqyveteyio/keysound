#include "KeySoundApo.h"
#include "MediaType.h"

#include <math.h>
#include <string.h>
#include <wchar.h>
#include <new>

// dllmain.cpp 里的模块引用计数，DllCanUnloadNow 要用
void ModuleAddRef();
void ModuleRelease();

// 老 SDK 里没有这个错误码，退回一个语义最接近的
#ifndef APOERR_FORMAT_NOT_SUPPORTED
#include <audioclient.h>
#define APOERR_FORMAT_NOT_SUPPORTED AUDCLNT_E_UNSUPPORTED_FORMAT
#endif

static const float kTestToneHz = 440.0f;
static const float kTestToneGain = 0.25f;
static const float kTwoPi = 6.283185307179586f;

CKeySoundApo::CKeySoundApo()
    : ref_(1),
      initialized_(false),
      locked_(false),
      sample_rate_(0),
      channels_(0),
      max_frames_(0),
      mix_buffer_(NULL),
      test_tone_(false),
      tone_phase_(0.0f) {
    ModuleAddRef();
}

CKeySoundApo::~CKeySoundApo() {
    UnlockForProcess();
    ModuleRelease();
}

STDMETHODIMP CKeySoundApo::QueryInterface(REFIID riid, void** ppv) {
    if (ppv == NULL) {
        return E_POINTER;
    }
    if (riid == __uuidof(IAudioProcessingObject) || riid == __uuidof(IUnknown)) {
        *ppv = static_cast<IAudioProcessingObject*>(this);
    } else if (riid == __uuidof(IAudioProcessingObjectRT)) {
        *ppv = static_cast<IAudioProcessingObjectRT*>(this);
    } else if (riid == __uuidof(IAudioProcessingObjectConfiguration)) {
        *ppv = static_cast<IAudioProcessingObjectConfiguration*>(this);
    } else if (riid == __uuidof(IAudioSystemEffects)) {
        *ppv = static_cast<IAudioSystemEffects*>(this);
    } else {
        *ppv = NULL;
        return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
}

STDMETHODIMP_(ULONG) CKeySoundApo::AddRef() {
    return (ULONG)InterlockedIncrement(&ref_);
}

STDMETHODIMP_(ULONG) CKeySoundApo::Release() {
    const LONG left = InterlockedDecrement(&ref_);
    if (left == 0) {
        delete this;
    }
    return (ULONG)left;
}

STDMETHODIMP CKeySoundApo::Reset() {
    tone_phase_ = 0.0f;
    return S_OK;
}

STDMETHODIMP CKeySoundApo::GetLatency(HNSTIME* time) {
    if (time == NULL) {
        return E_POINTER;
    }
    // 就地相加，不引入额外延迟
    *time = 0;
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
    out->Flags = (APO_FLAG)(APO_FLAG_INPLACE |
                            APO_FLAG_SAMPLESPERFRAME_MUST_MATCH |
                            APO_FLAG_FRAMESPERSECOND_MUST_MATCH |
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
    return S_OK;
}

STDMETHODIMP CKeySoundApo::Initialize(UINT32 data_size, BYTE* data) {
    UNREFERENCED_PARAMETER(data_size);
    UNREFERENCED_PARAMETER(data);
    // 我们不需要 INF 里带初始化数据，给什么都照单全收
    initialized_ = true;
    return S_OK;
}

// 只认 32 位浮点，采样率和声道数听音频引擎的。
// 这里放得越松越好：卡得太严会让麦克风整个不工作（实现方案.md 第 9 节症状对照）
HRESULT CKeySoundApo::NegotiateFormat(IAudioMediaType* requested, IAudioMediaType** supported) {
    if (supported == NULL) {
        return E_POINTER;
    }
    *supported = NULL;
    if (requested == NULL) {
        return E_POINTER;
    }
    const WAVEFORMATEX* format = requested->GetAudioFormat();
    if (format == NULL) {
        return E_POINTER;
    }

    if (IsFloat32Format(format)) {
        CApoMediaType* copy = CApoMediaType::Create(format);
        if (copy == NULL) {
            return E_OUTOFMEMORY;
        }
        *supported = copy;
        return S_OK;
    }

    // 不支持的格式：按文档给出最接近的那个，返回 S_FALSE
    CApoMediaType* fallback = CApoMediaType::CreateFloat32(
        format->nSamplesPerSec, format->nChannels, 0);
    if (fallback == NULL) {
        return APOERR_FORMAT_NOT_SUPPORTED;
    }
    *supported = fallback;
    return S_FALSE;
}

STDMETHODIMP CKeySoundApo::IsInputFormatSupported(IAudioMediaType* opposite,
                                                  IAudioMediaType* requested,
                                                  IAudioMediaType** supported) {
    UNREFERENCED_PARAMETER(opposite);
    return NegotiateFormat(requested, supported);
}

STDMETHODIMP CKeySoundApo::IsOutputFormatSupported(IAudioMediaType* opposite,
                                                   IAudioMediaType* requested,
                                                   IAudioMediaType** supported) {
    UNREFERENCED_PARAMETER(opposite);
    return NegotiateFormat(requested, supported);
}

STDMETHODIMP CKeySoundApo::GetInputChannelCount(UINT32* channel_count) {
    if (channel_count == NULL) {
        return E_POINTER;
    }
    // 还没 LockForProcess 的时候声道数是未知的，但这里不能报错：
    // 有的宿主在协商之前就会问一次，返回失败会让整条链路建不起来
    *channel_count = channels_;
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
    if (!IsFloat32Format(format)) {
        return APOERR_FORMAT_NOT_SUPPORTED;
    }

    sample_rate_ = format->nSamplesPerSec;
    channels_ = format->nChannels;
    max_frames_ = input_connections[0]->u32MaxFrameCount;
    if (max_frames_ == 0) {
        max_frames_ = sample_rate_ / 10;  // 兜底 100ms，别让缓冲变成 0
    }

    mix_buffer_ = new (std::nothrow) float[(size_t)max_frames_ * channels_];
    if (mix_buffer_ == NULL) {
        return E_OUTOFMEMORY;
    }

    // 这里是非实时线程，读注册表、建共享内存都只能放在这一步；
    // 也只有到了这里格式才最终定下来，头部才填得出采样率和声道数
    test_tone_ = ReadTestToneFlag();
    ring_.Create(sample_rate_, channels_);

    tone_phase_ = 0.0f;
    locked_ = true;
    return S_OK;
}

STDMETHODIMP CKeySoundApo::UnlockForProcess() {
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
    out->u32BufferFlags = in->u32BufferFlags;
    if (in->u32BufferFlags == BUFFER_INVALID || frames == 0 || frames > max_frames_) {
        return;
    }

    float* dst = reinterpret_cast<float*>(out->pBuffer);
    const float* src = reinterpret_cast<const float*>(in->pBuffer);
    if (dst == NULL) {
        return;
    }

    const size_t samples = (size_t)frames * channels_;
    // BUFFER_SILENT 时输入缓冲里是未定义内容，当成静音清零，
    // 不然会把垃圾数据当成麦克风信号放出去
    if (in->u32BufferFlags == BUFFER_SILENT) {
        memset(dst, 0, samples * sizeof(float));
    } else if (dst != src && src != NULL) {
        memcpy(dst, src, samples * sizeof(float));
    }

    UINT32 mixed = 0;
    if (test_tone_) {
        const float step = kTwoPi * kTestToneHz / (float)sample_rate_;
        float phase = tone_phase_;
        for (UINT32 f = 0; f < frames; ++f) {
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
        mixed = frames;
    } else if (mix_buffer_ != NULL) {
        mixed = ring_.Read(mix_buffer_, frames);
    }

    if (mixed == 0) {
        // 缓冲空是常态（没在放音效），什么都不加，麦克风完全不受影响
        return;
    }

    const size_t mixed_samples = (size_t)mixed * channels_;
    for (size_t i = 0; i < mixed_samples; ++i) {
        float value = dst[i] + mix_buffer_[i];
        // 软削波：连按时几个音效相加很容易过 1，直接溢出会变成刺啦声
        if (value > 1.0f) {
            value = 1.0f;
        } else if (value < -1.0f) {
            value = -1.0f;
        }
        dst[i] = value;
    }
    out->u32BufferFlags = BUFFER_VALID;
}
