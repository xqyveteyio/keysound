#pragma once

#include <windows.h>
#include <audioenginebaseapo.h>

#include "../common/KeySoundShared.h"
#include "RingBuffer.h"

// 采集方向的系统效果 APO：把 KeySound 从共享内存喂过来的音效加到麦克风采集流上。
// 只实现最基本的四个接口，不用 WDK 的 CBaseAudioProcessingObject 基类，
// 这样 CI 上只要 MSVC + Windows SDK 就能编（实现方案.md 7.1）。
class CKeySoundApo : public IAudioProcessingObject,
                     public IAudioProcessingObjectRT,
                     public IAudioProcessingObjectConfiguration,
                     public IAudioSystemEffects {
public:
    CKeySoundApo();

    // IUnknown
    STDMETHOD(QueryInterface)(REFIID riid, void** ppv);
    STDMETHOD_(ULONG, AddRef)();
    STDMETHOD_(ULONG, Release)();

    // IAudioProcessingObject
    STDMETHOD(Reset)();
    STDMETHOD(GetLatency)(HNSTIME* time);
    STDMETHOD(GetRegistrationProperties)(APO_REG_PROPERTIES** props);
    STDMETHOD(Initialize)(UINT32 data_size, BYTE* data);
    STDMETHOD(IsInputFormatSupported)(IAudioMediaType* opposite,
                                      IAudioMediaType* requested,
                                      IAudioMediaType** supported);
    STDMETHOD(IsOutputFormatSupported)(IAudioMediaType* opposite,
                                       IAudioMediaType* requested,
                                       IAudioMediaType** supported);
    STDMETHOD(GetInputChannelCount)(UINT32* channel_count);

    // IAudioProcessingObjectRT
    STDMETHOD_(void, APOProcess)(UINT32 num_input_connections,
                                 APO_CONNECTION_PROPERTY** input_connections,
                                 UINT32 num_output_connections,
                                 APO_CONNECTION_PROPERTY** output_connections);
    STDMETHOD_(UINT32, CalcInputFrames)(UINT32 output_frame_count);
    STDMETHOD_(UINT32, CalcOutputFrames)(UINT32 input_frame_count);

    // IAudioProcessingObjectConfiguration
    STDMETHOD(LockForProcess)(UINT32 num_input_connections,
                              APO_CONNECTION_DESCRIPTOR** input_connections,
                              UINT32 num_output_connections,
                              APO_CONNECTION_DESCRIPTOR** output_connections);
    STDMETHOD(UnlockForProcess)();

private:
    ~CKeySoundApo();

    HRESULT NegotiateFormat(IAudioMediaType* requested, IAudioMediaType** supported);
    // 只有非实时方法能读注册表，测试音开关在 LockForProcess 里取一次
    static bool ReadTestToneFlag();

    LONG ref_;
    bool initialized_;
    bool locked_;

    UINT32 sample_rate_;
    UINT32 channels_;
    UINT32 max_frames_;

    float* mix_buffer_;   // LockForProcess 里一次分配好，APOProcess 里绝不分配
    RingConsumer ring_;

    bool test_tone_;      // Phase 0：无条件注入 440Hz 正弦波
    float tone_phase_;
};
