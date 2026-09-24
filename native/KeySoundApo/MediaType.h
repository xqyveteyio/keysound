// 自己实现的 IAudioMediaType。
// SDK 里的 CreateAudioMediaType() 要链 WDK 的 AudioEng.lib，为了让 CI 上只装
// MSVC + Windows SDK 就能编（实现方案.md 7.1），这里手写一个，反正只有几十行。
#pragma once

#include <windows.h>
#include <objbase.h>
#include <mmreg.h>
#include <ks.h>
#include <ksmedia.h>
#include <audioenginebaseapo.h>
#include <new>

static const GUID kSubtypeIeeeFloat = { STATIC_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT };
static const GUID kSubtypePcm = { STATIC_KSDATAFORMAT_SUBTYPE_PCM };
// 自由线程标记。audiodg 会把格式对象交到另一个线程，没有它就无法封送，采集图建不起来
static const GUID kIidAgileObject = {
    0x94ea2b94, 0xe9cc, 0x49e0, { 0xc0, 0xff, 0xee, 0x64, 0xca, 0x8f, 0x5b, 0x90 }
};

// APOProcess 里按这个分支混音。认不出来就原样透传，绝不能把麦克风链路建失败
enum KeySoundSampleKind {
    kSamplePassthrough = 0,
    kSampleFloat32 = 1,
    kSamplePcm16 = 2,
    kSamplePcm32 = 3,
    kSamplePcm24In32 = 4,  // 32 位容器里放 24 个有效位，高位对齐
    kSamplePcm24Packed = 5
};

class CApoMediaType : public IAudioMediaType {
public:
    // 复制一份 WAVEFORMATEX（含 cbSize 后面的扩展字节），失败返回 NULL
    static CApoMediaType* Create(const WAVEFORMATEX* format) {
        if (format == NULL) {
            return NULL;
        }
        // cbSize 异常大时别照单全收，否则 memcpy 会读越界，audiodg 一崩整只麦克风就没了
        size_t extra = format->cbSize;
        if (extra > 512) {
            extra = 512;
        }
        const size_t size = sizeof(WAVEFORMATEX) + extra;
        BYTE* copy = static_cast<BYTE*>(CoTaskMemAlloc(size));
        if (copy == NULL) {
            return NULL;
        }
        memcpy(copy, format, size);
        reinterpret_cast<WAVEFORMATEX*>(copy)->cbSize = (WORD)extra;
        CApoMediaType* self = new (std::nothrow) CApoMediaType(reinterpret_cast<WAVEFORMATEX*>(copy));
        if (self == NULL) {
            CoTaskMemFree(copy);
        }
        return self;
    }

    // 按给定的采样率/声道数造一个 float32 的 WAVEFORMATEXTENSIBLE，
    // 用来在 IsXxxFormatSupported 里给出「我们能接受的最接近的格式」
    static CApoMediaType* CreateFloat32(DWORD sample_rate, WORD channels, DWORD channel_mask) {
        WAVEFORMATEXTENSIBLE wfx = {};
        wfx.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
        wfx.Format.nChannels = channels;
        wfx.Format.nSamplesPerSec = sample_rate;
        wfx.Format.wBitsPerSample = 32;
        wfx.Format.nBlockAlign = (WORD)(channels * 4);
        wfx.Format.nAvgBytesPerSec = sample_rate * wfx.Format.nBlockAlign;
        wfx.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
        wfx.Samples.wValidBitsPerSample = 32;
        wfx.dwChannelMask = channel_mask;
        wfx.SubFormat = kSubtypeIeeeFloat;
        return Create(&wfx.Format);
    }

    // IUnknown
    STDMETHOD(QueryInterface)(REFIID riid, void** ppv) {
        if (ppv == NULL) {
            return E_POINTER;
        }
        if (free_marshaler_ != NULL &&
            (riid == __uuidof(IMarshal) || riid == kIidAgileObject)) {
            return free_marshaler_->QueryInterface(riid, ppv);
        }
        if (riid == __uuidof(IAudioMediaType) || riid == __uuidof(IUnknown)) {
            *ppv = static_cast<IAudioMediaType*>(this);
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

    // IAudioMediaType
    STDMETHOD(IsCompressedFormat)(BOOL* compressed) {
        if (compressed == NULL) {
            return E_POINTER;
        }
        *compressed = FALSE;
        return S_OK;
    }

    STDMETHOD(IsEqual)(IAudioMediaType* other, DWORD* flags) {
        if (flags == NULL) {
            return E_POINTER;
        }
        *flags = 0;
        if (other == NULL) {
            return E_POINTER;
        }
        const WAVEFORMATEX* rhs = other->GetAudioFormat();
        if (rhs == NULL) {
            return E_POINTER;
        }
        if (rhs->wFormatTag == format_->wFormatTag) {
            *flags |= AUDIOMEDIATYPE_EQUAL_FORMAT_TYPES;
        }
        if (rhs->cbSize == format_->cbSize &&
            memcmp(rhs, format_, sizeof(WAVEFORMATEX) + format_->cbSize) == 0) {
            *flags |= AUDIOMEDIATYPE_EQUAL_FORMAT_DATA | AUDIOMEDIATYPE_EQUAL_FORMAT_USER_DATA;
        }
        // 文档约定：完全一样返回 S_OK，有差异返回 S_FALSE
        const DWORD all = AUDIOMEDIATYPE_EQUAL_FORMAT_TYPES |
                          AUDIOMEDIATYPE_EQUAL_FORMAT_DATA |
                          AUDIOMEDIATYPE_EQUAL_FORMAT_USER_DATA;
        return (*flags == all) ? S_OK : S_FALSE;
    }

    STDMETHOD_(const WAVEFORMATEX*, GetAudioFormat)() {
        return format_;
    }

    STDMETHOD(GetUncompressedAudioFormat)(UNCOMPRESSEDAUDIOFORMAT* out) {
        if (out == NULL) {
            return E_POINTER;
        }
        DWORD channel_mask = 0;
        DWORD valid_bits = format_->wBitsPerSample;
        if (format_->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
            format_->cbSize >= sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX)) {
            const WAVEFORMATEXTENSIBLE* ext =
                reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(format_);
            channel_mask = ext->dwChannelMask;
            if (ext->Samples.wValidBitsPerSample != 0) {
                valid_bits = ext->Samples.wValidBitsPerSample;
            }
        }
        // 必须和 WAVEFORMATEX 里的子类型一致。报成浮点、波形却是 PCM 的话，
        // 引擎会认为格式对不上，直接把整条采集图拆掉
        GUID subtype = kSubtypeIeeeFloat;
        if (format_->wFormatTag == WAVE_FORMAT_PCM) {
            subtype = kSubtypePcm;
        } else if (format_->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
                   format_->cbSize >= sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX)) {
            subtype = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(format_)->SubFormat;
        }
        out->guidFormatType = subtype;
        out->dwSamplesPerFrame = format_->nChannels;
        out->dwBytesPerSampleContainer = format_->wBitsPerSample / 8;
        out->dwValidBitsPerSample = valid_bits;
        out->fFramesPerSecond = (FLOAT)format_->nSamplesPerSec;
        out->dwChannelMask = channel_mask;
        return S_OK;
    }

private:
    explicit CApoMediaType(WAVEFORMATEX* owned) : ref_(1), format_(owned), free_marshaler_(NULL) {
        // 格式对象会被交到另一个线程。没有自由线程封送器时，建图在封送这一步失败
        CoCreateFreeThreadedMarshaler(
            static_cast<IUnknown*>(static_cast<IAudioMediaType*>(this)), &free_marshaler_);
    }
    ~CApoMediaType() {
        if (free_marshaler_ != NULL) {
            free_marshaler_->Release();
        }
        CoTaskMemFree(format_);
    }

    CApoMediaType(const CApoMediaType&);
    CApoMediaType& operator=(const CApoMediaType&);

    LONG ref_;
    WAVEFORMATEX* format_;
    IUnknown* free_marshaler_;
};

// 这个格式我们能不能直接处理：音频引擎内部一律是 32 位浮点
inline bool IsFloat32Format(const WAVEFORMATEX* format) {
    if (format == NULL || format->wBitsPerSample != 32 || format->nChannels == 0) {
        return false;
    }
    if (format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
        return true;
    }
    if (format->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
        format->cbSize >= sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX)) {
        const WAVEFORMATEXTENSIBLE* ext =
            reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(format);
        return IsEqualGUID(ext->SubFormat, kSubtypeIeeeFloat) != 0;
    }
    return false;
}

// 复合效果列表里前面已经挂着 Realtek 的 APO，两边必须接受同一种格式。
// 麦克风阵列经常是 16 位 PCM 而不是浮点，这里认不出就返回透传，调用方不得改格式
inline KeySoundSampleKind ClassifyFormat(const WAVEFORMATEX* format) {
    if (format == NULL || format->nChannels == 0 || format->nChannels > 8 ||
        format->nSamplesPerSec == 0) {
        return kSamplePassthrough;
    }
    const bool extensible =
        format->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
        format->cbSize >= sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
    GUID subtype = GUID_NULL;
    WORD valid_bits = format->wBitsPerSample;
    if (extensible) {
        const WAVEFORMATEXTENSIBLE* ext =
            reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(format);
        subtype = ext->SubFormat;
        if (ext->Samples.wValidBitsPerSample != 0) {
            valid_bits = ext->Samples.wValidBitsPerSample;
        }
    }
    const bool is_float = format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT ||
                          (extensible && IsEqualGUID(subtype, kSubtypeIeeeFloat));
    const bool is_pcm = format->wFormatTag == WAVE_FORMAT_PCM ||
                        (extensible && IsEqualGUID(subtype, kSubtypePcm));
    if (is_float && format->wBitsPerSample == 32) {
        return kSampleFloat32;
    }
    if (is_pcm && format->wBitsPerSample == 16) {
        return kSamplePcm16;
    }
    if (is_pcm && format->wBitsPerSample == 32 && valid_bits > 0 && valid_bits <= 24) {
        return kSamplePcm24In32;
    }
    if (is_pcm && format->wBitsPerSample == 32) {
        return kSamplePcm32;
    }
    if (is_pcm && format->wBitsPerSample == 24) {
        return kSamplePcm24Packed;
    }
    return kSamplePassthrough;
}
