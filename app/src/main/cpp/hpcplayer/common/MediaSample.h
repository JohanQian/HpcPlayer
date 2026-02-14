#pragma once

#include <cstdint>
#include <memory>
#include <vector>

/**
 * @brief Represents a single compressed media sample (e.g., a video NAL unit or an audio frame).
 *
 * This struct holds the raw, compressed data read from the extractor before it is sent
 * to the decoder.
 */
struct MediaSample {
    
    // The compressed data buffer. Access the raw pointer via .data() and size via .size().
    std::vector<uint8_t> data;

    // Presentation timestamp in microseconds.
    int64_t pts = 0;

    // Index of the track this sample belongs to.
    int track_index = -1;

    // True if this is the last sample in the stream.
    bool is_eos = false;

    bool is_seek_frame = false;
};
