#include "MediaCodecAudioDecoder.h"
#include <android/log.h>
#include <string.h>

namespace hpc {

MediaCodecAudioDecoder::MediaCodecAudioDecoder() : MediaCodecDecoder() {}

status_t MediaCodecAudioDecoder::configureCodec(AMediaFormat* format, const MetaData& meta) {
  // Set audio-specific parameters
  AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_SAMPLE_RATE, meta.sampleRate);
  AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_CHANNEL_COUNT, meta.channels);
  // Add other audio-specific parameters (e.g., pcm-encoding) as needed
  return OK;
}

status_t MediaCodecAudioDecoder::processOutputBuffer(AMediaCodecBufferInfo& info, size_t bufferIndex,
                                                     std::shared_ptr<MediaBuffer>& buffer) {
  size_t outSize;
  uint8_t* outData = AMediaCodec_getOutputBuffer(mCodec, bufferIndex, &outSize);
  if (!outData || outSize == 0) {
    AMediaCodec_releaseOutputBuffer(mCodec, bufferIndex, false);
    return ERROR_UNKNOWN;
  }

  // Create output buffer (PCM data for audio)
  buffer = std::make_shared<MediaBuffer>();
  buffer->data = std::shared_ptr<uint8_t>(new uint8_t[outSize], std::default_delete<uint8_t[]>());
  buffer->size = outSize;
  buffer->ptsUs = info.presentationTimeUs;
  buffer->isKeyFrame = false;  // Audio typically doesn't have keyframes
  buffer->isEOS = (info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) != 0;
  memcpy(buffer->data.get(), outData, outSize);

  // Release codec buffer
  AMediaCodec_releaseOutputBuffer(mCodec, bufferIndex, false);
  mStatus.currentTimeUs = info.presentationTimeUs;
  return buffer->isEOS ? ERROR_END_OF_STREAM : OK;
}

status_t MediaCodecAudioDecoder::seek(int64_t timeUs) {
  std::lock_guard<std::mutex> lock(mMutex);
  if (!mInitialized) return ERROR_UNKNOWN;

  // Flush codec and reset state
  status_t status = flush();
  if (status != OK) return status;

  mStatus.currentTimeUs = timeUs;
  return OK;
}

}  // namespace hpc