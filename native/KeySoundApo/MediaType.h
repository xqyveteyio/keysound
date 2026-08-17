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

class CApoMediaType : public IAudioMediaType {
public:
    // 复制一份 WAVEFORMATEX（含 cbSize 后面的扩展字节），失败返回 NULL
    static CApoMediaType* Create(const WAVEFORMATEX* format) {
        if (format == NULL) {
            return NULL;
        }
        const size_t size = sizeof(WAVEFORMATEX) + format->cbSize;
        BYTE* copy = static_cast<BYTE*>(CoTaskMemAlloc(size));
        if (copy == NULL) {
            return NULL;
        }
        memcpy(copy, format, size);
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
        out->guidFormatType = kSubtypeIeeeFloat;
        out->dwSamplesPerFrame = format_->nChannels;
        out->dwBytesPerSampleContainer = format_->wBitsPerSample / 8;
        out->dwValidBitsPerSample = valid_bits;
        out->fFramesPerSecond = (FLOAT)format_->nSamplesPerSec;
        out->dwChannelMask = channel_mask;
        return S_OK;
    }

private:
    explicit CApoMediaType(WAVEFORMATEX* owned) : ref_(1), format_(owned) {}
    ~CApoMediaType() { CoTaskMemFree(format_); }

    CApoMediaType(const CApoMediaType&);
    CApoMediaType& operator=(const CApoMediaType&);

    LONG ref_;
    WAVEFORMATEX* format_;
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
