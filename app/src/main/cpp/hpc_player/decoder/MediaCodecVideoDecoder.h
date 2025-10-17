#pragma once

#include "MediaCodecDecoder.h"

namespace hpc {

class MediaCodecVideoDecoder : public MediaCodecDecoder {
 public:
  MediaCodecVideoDecoder();
  ~MediaCodecVideoDecoder() override = default;

  // Seek to a specific position
  status_t seek(int64_t timeUs) override;

 protected:
  // Configure MediaCodec with video-specific parameters
  status_t configureCodec(AMediaFormat* format, const MetaData& meta) override;

  // Process video output buffer
  status_t processOutputBuffer(AMediaCodecBufferInfo& info, size_t bufferIndex,
                               std::shared_ptr<MediaBuffer>& buffer) override;
};

}  // namespace hpc