#pragma once

#include "DecoderBase.h"
#include <media/NdkMediaCodec.h>  // MediaCodec NDK API
#include <media/NdkMediaFormat.h>

namespace hpc {

class MediaCodecDecoder : public Decoder {
 public:
  // Constructor: enable async mode by default
  MediaCodecDecoder();
  ~MediaCodecDecoder() override;

  // Initialize the decoder with metadata
  status_t init(const MetaData& meta) override;

  // Feed input buffer to codec
  status_t input(const std::shared_ptr<MediaBuffer>& buffer) override;

  // Retrieve decoded frame
  status_t output(std::shared_ptr<MediaBuffer>& buffer) override;

  // Flush the decoder
  status_t flush() override;

  // Release resources
  void release() override;

 protected:
  // Handle format change (virtual for subclasses to override)
  status_t onFormatChanged(const MetaData& newMeta) override;

  // Configure MediaCodec with metadata (to be specialized by subclasses)
  virtual status_t configureCodec(AMediaFormat* format, const MetaData& meta) = 0;

  // Process output buffer info (to be specialized by subclasses)
  virtual status_t processOutputBuffer(AMediaCodecBufferInfo& info, size_t bufferIndex,
                                       std::shared_ptr<MediaBuffer>& buffer) = 0;

  AMediaCodec* mCodec;  // MediaCodec instance
};

}  // namespace hpc