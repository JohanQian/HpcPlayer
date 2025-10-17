#include "MediaCodecDecoder.h"
#include <android/log.h>
#include <string.h>

namespace hpc {

MediaCodecDecoder::MediaCodecDecoder() : Decoder(true), mCodec(nullptr) {}

MediaCodecDecoder::~MediaCodecDecoder() {
  release();
}

status_t MediaCodecDecoder::init(const MetaData& meta) {
  std::lock_guard<std::mutex> lock(mMutex);
  if (mInitialized) return OK;

  mMeta = meta;
  mCodec = AMediaCodec_createDecoderByType(meta.mimeType.c_str());
  if (!mCodec) {
    __android_log_print(ANDROID_LOG_ERROR, "MediaCodecDecoder", "Failed to create codec for %s",
                        meta.mimeType.c_str());
    return ERROR_INVALID_FORMAT;
  }

  // Create and configure MediaFormat
  AMediaFormat* format = AMediaFormat_new();
  AMediaFormat_setString(format, AMEDIAFORMAT_KEY_MIME, meta.mimeType.c_str());
  AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_BIT_RATE, meta.bitrate);

  // Let subclass configure specific parameters
  status_t status = configureCodec(format, meta);
  if (status != OK) {
    AMediaFormat_delete(format);
    AMediaCodec_delete(mCodec);
    mCodec = nullptr;
    return status;
  }

  // Configure codec in async decode mode
  media_status_t codecStatus = AMediaCodec_configure(mCodec, format, nullptr /* surface */,
                                                     nullptr /* crypto */, AMEDIACODEC_CONFIGURE_FLAG_ENCODE);
  AMediaFormat_delete(format);
  if (codecStatus != AMEDIA_OK) {
    __android_log_print(ANDROID_LOG_ERROR, "MediaCodecDecoder", "Failed to configure codec");
    AMediaCodec_delete(mCodec);
    mCodec = nullptr;
    return ERROR_INVALID_FORMAT;
  }

  codecStatus = AMediaCodec_start(mCodec);
  if (codecStatus != AMEDIA_OK) {
    __android_log_print(ANDROID_LOG_ERROR, "MediaCodecDecoder", "Failed to start codec");
    AMediaCodec_delete(mCodec);
    mCodec = nullptr;
    return ERROR_UNKNOWN;
  }

  mInitialized = true;
  mStatus.isDecoding = true;
  return OK;
}

status_t MediaCodecDecoder::input(const std::shared_ptr<MediaBuffer>& buffer) {
  std::lock_guard<std::mutex> lock(mMutex);
  if (!mInitialized) return ERROR_UNKNOWN;

  // Get available input buffer
  ssize_t inputIndex = AMediaCodec_dequeueInputBuffer(mCodec, 10000 /* timeoutUs */);
  if (inputIndex < 0) {
    __android_log_print(ANDROID_LOG_WARN, "MediaCodecDecoder", "No input buffer available: %zd", inputIndex);
    return ERROR_BUFFER_FULL;
  }

  size_t bufferSize;
  uint8_t* inputData = AMediaCodec_getInputBuffer(mCodec, inputIndex, &bufferSize);
  if (!inputData || bufferSize < buffer->size) {
    return ERROR_BUFFER_FULL;
  }

  // Copy data to codec input buffer
  memcpy(inputData, buffer->data.get(), buffer->size);
  uint32_t flags = buffer->isEOS ? AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM : 0;
  media_status_t status = AMediaCodec_queueInputBuffer(mCodec, inputIndex, 0 /* offset */, buffer->size,
                                                       buffer->ptsUs, flags);
  if (status != AMEDIA_OK) {
    __android_log_print(ANDROID_LOG_ERROR, "MediaCodecDecoder", "Failed to queue input buffer");
    return ERROR_UNKNOWN;
  }

  mStatus.bufferedBytes += buffer->size;
  return OK;
}

status_t MediaCodecDecoder::output(std::shared_ptr<MediaBuffer>& buffer) {
  std::lock_guard<std::mutex> lock(mMutex);
  if (!mInitialized) return ERROR_UNKNOWN;

  AMediaCodecBufferInfo info;
  ssize_t outputIndex = AMediaCodec_dequeueOutputBuffer(mCodec, &info, 10000 /* timeoutUs */);
  if (outputIndex >= 0) {
    // Valid output buffer
    return processOutputBuffer(info, outputIndex, buffer);
  } else if (outputIndex == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
    // Handle format change
    AMediaFormat* format = AMediaCodec_getOutputFormat(mCodec);
    MetaData newMeta;
    char* mime;
    AMediaFormat_getString(format, AMEDIAFORMAT_KEY_MIME, &mime);
    newMeta.mimeType = mime;
    AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_WIDTH, &newMeta.width);
    AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_HEIGHT, &newMeta.height);
    AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_BIT_RATE, &newMeta.bitrate);
    AMediaFormat_delete(format);
    return onFormatChanged(newMeta);
  } else if (outputIndex == AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED) {
    // Deprecated in NDK, ignore
    return OK;
  } else if (outputIndex == AMEDIACODEC_INFO_TRY_AGAIN_LATER) {
    return ERROR_BUFFER_FULL;
  }

  __android_log_print(ANDROID_LOG_ERROR, "MediaCodecDecoder", "Unexpected output status: %zd", outputIndex);
  return ERROR_UNKNOWN;
}

status_t MediaCodecDecoder::flush() {
  std::lock_guard<std::mutex> lock(mMutex);
  if (!mInitialized) return OK;

  media_status_t status = AMediaCodec_flush(mCodec);
  if (status != AMEDIA_OK) {
    __android_log_print(ANDROID_LOG_ERROR, "MediaCodecDecoder", "Failed to flush codec");
    return ERROR_UNKNOWN;
  }

  mStatus.bufferedBytes = 0;
  mStatus.isDecoding = true;
  return OK;
}

void MediaCodecDecoder::release() {
  std::lock_guard<std::mutex> lock(mMutex);
  if (mCodec) {
    AMediaCodec_stop(mCodec);
    AMediaCodec_delete(mCodec);
    mCodec = nullptr;
  }
  mInitialized = false;
  mStatus = DecoderStatus();
}

status_t MediaCodecDecoder::onFormatChanged(const MetaData& newMeta) {
  // Default implementation: update metadata
  mMeta = newMeta;
  return OK;
}

}  // namespace hpc