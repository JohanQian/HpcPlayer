#pragma once

#include <cstdint>
#include <memory>
#include "Error.h"

namespace hpc {

class MediaPacket;
class MetaData;

class Extractor {
 public:
  struct TrackInfo {
    std::string mime_type;
    int width = 0;  // For video
    int height = 0; // For video
    int sample_rate = 0; // For audio
    std::string language; // For audio/subtitles
    // Add other relevant fields
  };

  struct DrmInfo {
    std::string scheme;
    std::string license_url;
    // Add other DRM-related fields
  };

  virtual status_t init(const char* url) = 0;
  virtual int read(std::unique_ptr<MediaPacket> &packet, int index) = 0;
  virtual status_t seek(int64_t position) = 0;
  virtual void flush() = 0;
  virtual void getMetaData(MetaData& meta) = 0;
  virtual void release() = 0;

//  // Suggested additions
//  virtual status_t getTrackCount(int& count) = 0;
//  virtual status_t getTrackInfo(int index, TrackInfo& info) = 0;
//  virtual status_t checkFormatChange(bool& formatChanged) = 0;

  virtual ~Extractor() = default;
};

} // hpc

