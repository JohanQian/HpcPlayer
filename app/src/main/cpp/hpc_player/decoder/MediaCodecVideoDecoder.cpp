#include "MediaCodecVideoDecoder.h"
#include <android/log.h>
#include <string.h>

namespace hpc {

MediaCodecVideoDecoder::MediaCodecVideoDecoder() : MediaCodecDecoder() {}

status_t MediaCodecVideoDecoder::configureCodec(AMediaFormat* format, const MetaData& meta) {
  // Set video-specific parameters
  AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_WIDTH, meta.width);
  AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_HEIGHT, meta.height);
  // Add other video-specific parameters (e.g., max-input-size, color-format) as needed
  return OK;
}

status_t MediaCodecVideoDecoder::processOutputBuffer(AMediaCodecBufferInfo& info, size_t bufferIndex,
                                                     std::shared_ptr<MediaBuffer>& buffer) {
  size_t outSize;
  uint8_t* outData = AMediaCodec_getOutputBuffer(mCodec, bufferIndex, &outSize);
  if (!outData || outSize == 0) {
    AMediaCodec_releaseOutputBuffer(mCodec, bufferIndex, false);
    return ERROR_UNKNOWN;
  }

  // Create output buffer
  buffer = std::make_shared<MediaBuffer>();
  buffer->data = std::shared_ptr<uint8_t>(new uint8_t[outSize], std::default_delete<uint8_t[]>());
  buffer->size = outSize;
  buffer->ptsUs = info.presentationTimeUs;
  buffer->isKeyFrame = (info.flags & AMEDIACODEC_BUFFER_FLAG_KEY_FRAME) != 0;
  buffer->isEOS = (info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) != 0;
  memcpy(buffer->data.get(), outData, outSize);

  // Release codec buffer (render=false, as output is copied to MediaBuffer)
  AMediaCodec_releaseOutputBuffer(mCodec, bufferIndex, false);
  mStatus.currentTimeUs = info.presentationTimeUs;
  return buffer->isEOS ? ERROR_END_OF_STREAM : OK;
}

status_t MediaCodecVideoDecoder::seek(int64_t timeUs) {
  std::lock_guard<std::mutex> lock(mMutex);
  if (!mInitialized) return ERROR_UNKNOWN;

  // Flush codec and reset state
  status_t status = flush();
  if (status != OK) return status;

  mStatus.currentTimeUs = timeUs;
  return OK;
}

}  // namespace hpc