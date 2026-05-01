#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace hpc {

/**
 * @brief A C-style struct to hold media format information.
 * All members are public for direct access.
 */
struct MediaFormat {
    // MIME type of the content (e.g., "video/avc", "audio/mp4a-latm").
    std::string mime_type;

    // --- Video specific ---
    int width = 0;
    int height = 0;

    // --- Audio specific ---
    int sample_rate = 0;
    int channels = 0;
    
    // --- Codec specific ---
    int codec_id = 0; // e.g., AV_CODEC_ID_H264

    // Codec-specific data (e.g., H.264 SPS/PPS).
    // Using a vector for automatic memory management.
    std::vector<uint8_t> extradata;
};

} // namespace hpc
