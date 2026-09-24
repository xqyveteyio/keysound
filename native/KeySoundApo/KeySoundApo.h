#pragma once

#include <windows.h>
#include <audioenginebaseapo.h>

#include "../common/KeySoundShared.h"
#include "RingBuffer.h"

// 加载阶段写到 Global\KeySoundApoTrace。stage 含义见 KeySoundApo.cpp 里的注释
void KeySoundTrace(UINT32 stage, const WAVEFORMATEX* format, UINT32 kind);
void KeySoundTraceGuid(UINT32 stage, REFGUID guid);

// 采集方向的系统效果 APO：把 KeySound 从共享内存喂过来的音效加到麦克风采集流上。
// 只实现最基本的四个接口，不用 WDK 的 CBaseAudioProcessingObject 基类，
// 这样 CI 上只要 MSVC + Windows SDK 就能编（实现方案.md 7.1）。
class CKeySoundApo : public IAudioProcessingObject,
                     public IAudioProcessingObjectRT,
                     public IAudioProcessingObjectConfiguration,
                     public IAudioSystemEffects2 {
public:
    // audiodg 创建 sAPO 时会传入外层 IUnknown。不接聚合的话类厂直接失败，麦克风打不开
    explicit CKeySoundApo(IUnknown* outer);

    // 这些 IUnknown 方法会转给外层对象。没人聚合时外层就是下面的 inner_
    STDMETHOD(QueryInterface)(REFIID riid, void** ppv);
    STDMETHOD_(ULONG, AddRef)();
    STDMETHOD_(ULONG, Release)();

    // 非委托的 IUnknown：引用计数和接口查询都走这里，避免和聚合方互相递归
    HRESULT InternalQueryInterface(REFIID riid, void** ppv);
    ULONG InternalAddRef();
    ULONG InternalRelease();

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

    // IAudioSystemEffects2。Windows 11 会问支持哪些效果，没有这个接口就停在协商之后
    STDMETHOD(GetEffectsList)(LPGUID* effects, UINT* count, HANDLE event);

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

    // 聚合时外部只能拿到这个内部 IUnknown，不能拿到会转发出去的那个
    class CInnerUnknown : public IUnknown {
    public:
        explicit CInnerUnknown(CKeySoundApo* parent) : parent_(parent) {}
        STDMETHOD(QueryInterface)(REFIID riid, void** ppv) {
            return parent_->InternalQueryInterface(riid, ppv);
        }
        STDMETHOD_(ULONG, AddRef)() { return parent_->InternalAddRef(); }
        STDMETHOD_(ULONG, Release)() { return parent_->InternalRelease(); }
        CKeySoundApo* parent_;
    };

    HRESULT NegotiateFormat(IAudioMediaType* opposite, IAudioMediaType* requested,
                            IAudioMediaType** supported);
    // 只有非实时方法能读注册表，测试音开关在 LockForProcess 里取一次
    static bool ReadTestToneFlag();

    CInnerUnknown inner_;
    IUnknown* outer_;
    IUnknown* free_marshaler_;  // 自由线程封送器，跨套间时交给它处理 IMarshal

    LONG ref_;
    bool initialized_;
    bool locked_;

    UINT32 sample_rate_;
    UINT32 channels_;
    UINT32 negotiated_channels_;  // Lock 之前 GetInputChannelCount 不能回 0，否则图建不起来
    UINT32 max_frames_;
    UINT32 sample_kind_;      // KeySoundSampleKind，认不出就透传
    UINT32 bytes_per_frame_;  // 静音时按这个清零，不能拿浮点宽度去套整数格式

    float* mix_buffer_;   // LockForProcess 里一次分配好，APOProcess 里绝不分配
    RingConsumer ring_;

    // 原来占着这个槽位的厂商 APO。引擎只加载我们，厂商效果在 APOProcess 里先被调用，
    // 不能和它并排写进复合列表，否则两边抢同一种格式，麦克风原声会变成静音
    IAudioProcessingObject* child_apo_;
    IAudioProcessingObjectRT* child_rt_;
    IAudioProcessingObjectConfiguration* child_cfg_;

    bool test_tone_;      // Phase 0：无条件注入 440Hz 正弦波
    float tone_phase_;

    void ResetChild();
    void LoadChild(UINT32 data_size, BYTE* data);
};
