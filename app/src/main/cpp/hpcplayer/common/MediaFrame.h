#pragma once

#include <cstdint>

struct AMediaCodec;

struct MediaFrame {
    virtual ~MediaFrame() = default;

    int64_t pts = 0;
    int64_t index = -1;

    std::unique_ptr<uint8_t[]> data;
    int size = 0;
    int sample_rate = 0;
    int channels = 0;

    AMediaCodec* codec = nullptr;
    size_t buffer_index = 0;
    int64_t render_time_ns = 0;
    bool is_seek_frame = false;
};
