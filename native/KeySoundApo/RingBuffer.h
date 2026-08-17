// 共享内存环形缓冲的消费者端（跑在 audiodg.exe 里）。
// 单生产者（KeySound）单消费者（APO），无锁：只有生产者动 write_index，
// 只有消费者动 read_index，两边都只读对方那个下标。
#pragma once

#include <sddl.h>
#include <string.h>

#include "../common/KeySoundShared.h"

class RingConsumer {
public:
    RingConsumer() : mapping_(NULL), header_(NULL), data_(NULL), owner_(false) {}
    ~RingConsumer() { Close(); }

    // 由 APO 创建（audiodg 跑在 LOCAL SERVICE 下，有 SeCreateGlobalPrivilege），
    // KeySound 那边只 OpenFileMapping。反过来是不行的
    bool Create(uint32_t sample_rate, uint32_t channels) {
        Close();
        if (channels == 0 || channels > KEYSOUND_RING_MAX_CHANNELS) {
            return false;
        }

        SECURITY_ATTRIBUTES sa = {};
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = FALSE;
        if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
                KEYSOUND_RING_SDDL, SDDL_REVISION_1, &sa.lpSecurityDescriptor, NULL)) {
            sa.lpSecurityDescriptor = NULL;
        }

        mapping_ = CreateFileMappingW(INVALID_HANDLE_VALUE,
                                      sa.lpSecurityDescriptor ? &sa : NULL,
                                      PAGE_READWRITE, 0, kKeySoundRingBytes,
                                      KEYSOUND_RING_NAME);
        const DWORD create_error = GetLastError();
        if (sa.lpSecurityDescriptor) {
            LocalFree(sa.lpSecurityDescriptor);
        }
        if (mapping_ == NULL) {
            return false;
        }

        void* view = MapViewOfFile(mapping_, FILE_MAP_ALL_ACCESS, 0, 0, 0);
        if (view == NULL) {
            CloseHandle(mapping_);
            mapping_ = NULL;
            return false;
        }
        header_ = static_cast<RingHeader*>(view);
        data_ = reinterpret_cast<float*>(static_cast<BYTE*>(view) + sizeof(RingHeader));

        if (create_error != ERROR_ALREADY_EXISTS) {
            ZeroMemory(header_, sizeof(RingHeader));
        }

        // 抢消费者身份。段是别人（另一个 APO 实例）先建的时候就老实透传
        owner_ = (InterlockedCompareExchange(
                      reinterpret_cast<volatile LONG*>(&header_->consumer_claimed), 1, 0) == 0);
        if (!owner_) {
            return true;
        }

        uint32_t capacity = sample_rate / 1000 * KEYSOUND_RING_MILLIS;
        if (capacity > KEYSOUND_RING_MAX_FRAMES) {
            capacity = KEYSOUND_RING_MAX_FRAMES;
        }
        if (capacity < 2) {
            capacity = 2;
        }

        header_->read_index = 0;
        header_->write_index = 0;
        header_->capacity = capacity;
        header_->channels = channels;
        header_->sample_rate = sample_rate;
        header_->version = KEYSOUND_RING_VERSION;
        MemoryBarrier();
        // magic 最后写：Python 侧看到 magic 对了，才认为其余字段是可信的
        header_->magic = KEYSOUND_RING_MAGIC;
        return true;
    }

    void Close() {
        if (header_) {
            if (owner_) {
                header_->magic = 0;
                MemoryBarrier();
                InterlockedExchange(
                    reinterpret_cast<volatile LONG*>(&header_->consumer_claimed), 0);
            }
            UnmapViewOfFile(header_);
            header_ = NULL;
            data_ = NULL;
        }
        if (mapping_) {
            CloseHandle(mapping_);
            mapping_ = NULL;
        }
        owner_ = false;
    }

    bool active() const { return owner_ && header_ != NULL; }

    // 实时线程调用：只做算术和 memcpy，不加锁不分配
    void Heartbeat() {
        if (header_) {
            header_->apo_alive++;
        }
    }

    // 最多读 frames 帧到 dst（交织 float32），返回实际读到的帧数
    uint32_t Read(float* dst, uint32_t frames) {
        if (!owner_ || header_ == NULL || header_->magic != KEYSOUND_RING_MAGIC) {
            return 0;
        }
        const uint32_t capacity = header_->capacity;
        const uint32_t channels = header_->channels;
        if (capacity == 0 || channels == 0) {
            return 0;
        }

        const uint32_t read = header_->read_index % capacity;
        const uint32_t write = header_->write_index % capacity;
        MemoryBarrier();

        uint32_t avail = (write + capacity - read) % capacity;
        if (avail > frames) {
            avail = frames;
        }
        if (avail == 0) {
            return 0;
        }

        const uint32_t first = (avail > capacity - read) ? (capacity - read) : avail;
        memcpy(dst, data_ + (size_t)read * channels, (size_t)first * channels * sizeof(float));
        if (avail > first) {
            memcpy(dst + (size_t)first * channels, data_,
                   (size_t)(avail - first) * channels * sizeof(float));
        }

        MemoryBarrier();
        header_->read_index = (read + avail) % capacity;
        return avail;
    }

private:
    RingConsumer(const RingConsumer&);
    RingConsumer& operator=(const RingConsumer&);

    HANDLE mapping_;
    RingHeader* header_;
    float* data_;
    bool owner_;
};
