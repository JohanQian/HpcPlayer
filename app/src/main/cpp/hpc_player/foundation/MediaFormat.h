#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace hpc {

/**
 * @struct Format
 * @brief Encapsulates the format information of a media stream.
 */
struct Format {
  std::string mimeType;       // e.g., "video/avc", "audio/mp4a-latm"
  int32_t bitrate = -1;       // Bitrate

  // --- Video properties ---
  int32_t width = -1;
  int32_t height = -1;
  float frameRate = -1.0f;

  // --- Audio properties ---
  int32_t sampleRate = -1;    // Sample rate in Hz
  int32_t channelCount = -1;  // Number of audio channels

  // --- Codec-specific Data ---
  // e.g., H.264 sps/pps, AAC AudioSpecificConfig
  std::vector<uint8_t> csd;
};
}