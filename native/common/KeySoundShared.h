// KeySound 虚拟麦克风：APO 和注册工具、Python 侧共用的常量与共享内存布局。
// 这里的每一个值都在 实现方案.md 第 5 节里定死了，改动必须三边同步。
#pragma once

#include <windows.h>
#include <stdint.h>

// {F8E20E3A-DD0C-4119-9331-924D5E087417}
static const CLSID CLSID_KeySoundApo =
    { 0xf8e20e3a, 0xdd0c, 0x4119, { 0x93, 0x31, 0x92, 0x4d, 0x5e, 0x08, 0x74, 0x17 } };

#define KEYSOUND_APO_CLSID_STRING   L"{F8E20E3A-DD0C-4119-9331-924D5E087417}"
#define KEYSOUND_APO_FRIENDLY_NAME  L"KeySound Microphone Injector APO"
#define KEYSOUND_APO_COPYRIGHT      L"KeySound"

// 共享内存段名。必须建在 Global\ 里：audiodg.exe 跑在会话 0，
// KeySound 建在 Local\ 的东西它看不见（见 实现方案.md 7.3）
#define KEYSOUND_RING_NAME          L"Global\\KeySoundApoRing"

// 段的 DACL：只授交互用户读写。这里有个固有的权衡——本机其它用户态程序理论上
// 也能往用户的麦克风里塞音频；但换成 Everyone 全权限只会更糟
#define KEYSOUND_RING_SDDL          L"D:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;GA;;;LS)(A;;GRGW;;;IU)"

// 'KSA1'，Python 侧用它确认这段内存真是我们的
#define KEYSOUND_RING_MAGIC         0x3141534Bu
#define KEYSOUND_RING_VERSION       1u

// 段大小要固定：格式（采样率/声道数）变了也不能改已建好的段大小，
// 所以按最坏情况一次分配够，实际用多少看头部的 capacity
#define KEYSOUND_RING_MAX_FRAMES    19200u  // 400ms @ 48kHz
#define KEYSOUND_RING_MAX_CHANNELS  8u

// 缓冲多深。太小容易断音，太大则「停止播放」之后残留还会继续响
#define KEYSOUND_RING_MILLIS        200u

// Phase 0 的测试音开关放注册表里，省得为了试一下还要重编译。
// 值为 1 时 APO 无条件注入 440Hz 正弦波，不看共享内存
#define KEYSOUND_SETTINGS_KEY       L"SOFTWARE\\KeySound"
#define KEYSOUND_TESTTONE_VALUE     L"ApoTestTone"

#pragma pack(push, 4)
struct RingHeader {
    uint32_t magic;             // KEYSOUND_RING_MAGIC
    uint32_t version;           // KEYSOUND_RING_VERSION
    uint32_t sample_rate;       // APO 写：协商出来的采集采样率
    uint32_t channels;          // APO 写：协商出来的声道数
    uint32_t capacity;          // 数据区能放多少帧
    uint32_t apo_alive;         // APO 心跳，每次 APOProcess 自增
    uint32_t write_index;       // 帧下标，只有 KeySound 写
    uint32_t read_index;        // 帧下标，只有 APO 写
    // SFX 槽位是「每个应用流一份 APO」，同一台机器上可能同时活着好几个实例。
    // 谁先抢到这个标记谁负责消费，其余实例只透传，免得几个实例抢同一个 read_index
    uint32_t consumer_claimed;
    uint32_t reserved[7];
};
#pragma pack(pop)

// Python 侧是按 64 字节硬编码算数据区偏移的
static_assert(sizeof(RingHeader) == 64, "RingHeader 必须保持 64 字节");

static const uint32_t kKeySoundRingBytes =
    sizeof(RingHeader) +
    KEYSOUND_RING_MAX_FRAMES * KEYSOUND_RING_MAX_CHANNELS * (uint32_t)sizeof(float);
