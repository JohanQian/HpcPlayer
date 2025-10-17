#include "FFmpegVideoDecoder.h"

#include <cstring>
#include <vector>

namespace hpc {

namespace {

AVCodec* FindCodecByMime(const std::string& mime_type, bool is_video) {
  if (is_video) {
    if (mime_type == "video/avc") return avcodec_find_decoder(AV_CODEC_ID_H264);
    if (mime_type == "video/hevc") return avcodec_find_decoder(AV_CODEC_ID_HEVC);
    if (mime_type == "video/mpeg4") return avcodec_find_decoder(AV_CODEC_ID_MPEG4);
    return nullptr;
  } else {
    if (mime_type == "audio/mp4a-latm") return avcodec_find_decoder(AV_CODEC_ID_AAC);
    if (mime_type == "audio/mpeg") return avcodec_find_decoder(AV_CODEC_ID_MP3);
    if (mime_type == "audio/opus") return avcodec_find_decoder(AV_CODEC_ID_OPUS);
    return nullptr;
  }
}

}  // namespace

// FFmpegVideoDecoder implementation
FFmpegVideoDecoder::FFmpegVideoDecoder(bool async_mode)
    : Decoder(async_mode), frame_(av_frame_alloc()), packet_(av_packet_alloc()) {
}

FFmpegVideoDecoder::~FFmpegVideoDecoder() {
  FreeResources();
}

status_t FFmpegVideoDecoder::init(const MetaData& meta) {
  std::lock_guard<std::mutex> lock(mMutex);
  if (initialized_) {
    return OK;
  }

  codec_ = FindCodecByMime(meta.mimeType, true);
  if (!codec_) {
    return ERROR_INVALID_FORMAT;
  }

  codec_context_ = avcodec_alloc_context3(codec_);
  if (!codec_context_) {
    return ERROR_UNKNOWN;
  }

  codec_context_->width = meta.width;
  codec_context_->height = meta.height;
  codec_context_->bit_rate = meta.bitrate;

  if (avcodec_open2(codec_context_, codec_, nullptr) < 0) {
    avcodec_free_context(&codec_context_);
    return ERROR_INVALID_FORMAT;
  }

  mMeta = meta;
  initialized_ = true;
  mInitialized = true;
  return OK;
}

status_t FFmpegVideoDecoder::input(const std::shared_ptr<MediaBuffer>& buffer) {
  std::lock_guard<std::mutex> lock(mMutex);
  if (!initialized_) {
    return ERROR_INVALID_FORMAT;
  }

  av_packet_unref(packet_);
  packet_->data = buffer->data.get();
  packet_->size = buffer->size;
  packet_->pts = buffer->ptsUs;
  packet_->flags = buffer->isKeyFrame ? AV_PKT_FLAG_KEY : 0;

  int ret = avcodec_send_packet(codec_context_, packet_);
  if (ret < 0) {
    return ERROR_UNKNOWN;
  }
  mStatus.bufferedBytes += buffer->size;
  mStatus.isDecoding = true;
  return OK;
}

status_t FFmpegVideoDecoder::output(std::shared_ptr<MediaBuffer>& buffer) {
  std::lock_guard<std::mutex> lock(mMutex);
  if (!initialized_) {
    return ERROR_INVALID_FORMAT;
  }

  int ret = avcodec_receive_frame(codec_context_, frame_);
  if (ret == AVERROR_EOF) {
    buffer = std::make_shared<MediaBuffer>();
    buffer->isEOS = true;
    mStatus.isDecoding = false;
    return ERROR_END_OF_STREAM;
  } else if (ret < 0) {
    return ERROR_UNKNOWN;
  }

  size_t frame_size = frame_->width * frame_->height * 3 / 2;  // Assuming YUV420P
  buffer = std::make_shared<MediaBuffer>();
  buffer->data = std::shared_ptr<uint8_t>(static_cast<uint8_t*>(av_malloc(frame_size)), av_free);
  buffer->size = frame_size;
  buffer->ptsUs = frame_->pts;

  // Copy YUV data (simplified, assumes YUV420P)
  size_t offset = 0;
  for (int i = 0; i < 3; ++i) {
    size_t plane_size = frame_->linesize[i] * (i == 0 ? frame_->height : frame_->height / 2);
    std::memcpy(buffer->data.get() + offset, frame_->data[i], plane_size);
    offset += plane_size;
  }

  buffer->isKeyFrame = frame_->key_frame;
  mStatus.currentTimeUs = frame_->pts;
  mStatus.bufferedBytes = std::max(static_cast<ssize_t>(0), static_cast<ssize_t>(mStatus.bufferedBytes) - frame_size);
  return OK;
}

status_t FFmpegVideoDecoder::seek(int64_t time_us) {
  std::lock_guard<std::mutex> lock(mMutex);
  if (!initialized_) {
    return ERROR_INVALID_FORMAT;
  }

  flush();
  mStatus.currentTimeUs = time_us;
  return OK;
}

status_t FFmpegVideoDecoder::flush() {
  std::lock_guard<std::mutex> lock(mMutex);
  if (!initialized_) {
    return OK;
  }

  avcodec_flush_buffers(codec_context_);
  mStatus.bufferedBytes = 0;
  mStatus.isDecoding = false;
  return OK;
}

void FFmpegVideoDecoder::release() {
  std::lock_guard<std::mutex> lock(mMutex);
  FreeResources();
  initialized_ = false;
  mInitialized = false;
}

status_t FFmpegVideoDecoder::onFormatChanged(const MetaData& new_meta) {
  release();
  return init(new_meta);
}

void FFmpegVideoDecoder::FreeResources() {
  if (frame_) {
    av_frame_free(&frame_);
  }
  if (packet_) {
    av_packet_free(&packet_);
  }
  if (codec_context_) {
    avcodec_free_context(&codec_context_);
  }
  codec_ = nullptr;
}

bool FFmpegVideoDecoder::isSupportedMime(const std::string& mime_type) {
  return FindCodecByMime(mime_type, true) != nullptr;
}

// FFmpegAudioDecoder implementation
FFmpegAudioDecoder::FFmpegAudioDecoder(bool async_mode)
    : Decoder(async_mode), frame_(av_frame_alloc()), packet_(av_packet_alloc()) {
}

FFmpegAudioDecoder::~FFmpegAudioDecoder() {
  FreeResources();
}

status_t FFmpegAudioDecoder::init(const MetaData& meta) {
  std::lock_guard<std::mutex> lock(mMutex);
  if (initialized_) {
    return OK;
  }

  codec_ = FindCodecByMime(meta.mimeType, false);
  if (!codec_) {
    return ERROR_INVALID_FORMAT;
  }

  codec_context_ = avcodec_alloc_context3(codec_);
  if (!codec_context_) {
    return ERROR_UNKNOWN;
  }

  codec_context_->sample_rate = meta.sampleRate;
  codec_context_->channels = meta.channels;
  codec_context_->bit_rate = meta.bitrate;

  if (avcodec_open2(codec_context_, codec_, nullptr) < 0) {
    avcodec_free_context(&codec_context_);
    return ERROR_INVALID_FORMAT;
  }

  mMeta = meta;
  initialized_ = true;
  mInitialized = true;
  return OK;
}

status_t FFmpegAudioDecoder::input(const std::shared_ptr<MediaBuffer>& buffer) {
  std::lock_guard<std::mutex> lock(mMutex);
  if (!initialized_) {
    return ERROR_INVALID_FORMAT;
  }

  av_packet_unref(packet_);
  packet_->data = buffer->data.get();
  packet_->size = buffer->size;
  packet_->pts = buffer->ptsUs;
  packet_->flags = buffer->isKeyFrame ? AV_PKT_FLAG_KEY : 0;

  int ret = avcodec_send_packet(codec_context_, packet_);
  if (ret < 0) {
    return ERROR_UNKNOWN;
  }
  mStatus.bufferedBytes += buffer->size;
  mStatus.isDecoding = true;
  return OK;
}

status_t FFmpegAudioDecoder::output(std::shared_ptr<MediaBuffer>& buffer) {
  std::lock_guard<std::mutex> lock(mMutex);
  if (!initialized_) {
    return ERROR_INVALID_FORMAT;
  }

  int ret = avcodec_receive_frame(codec_context_, frame_);
  if (ret == AVERROR_EOF) {
    buffer = std::make_shared<MediaBuffer>();
    buffer->isEOS = true;
    mStatus.isDecoding = false;
    return ERROR_END_OF_STREAM;
  } else if (ret < 0) {
    return ERROR_UNKNOWN;
  }

  size_t frame_size = av_samples_get_buffer_size(nullptr, frame_->channels, frame_->nb_samples,
                                                 static_cast<AVSampleFormat>(frame_->format), 1);
  buffer = std::make_shared<MediaBuffer>();
  buffer->data = std::shared_ptr<uint8_t>(static_cast<uint8_t*>(av_malloc(frame_size)), av_free);
  buffer->size = frame_size;
  buffer->ptsUs = frame_->pts;

  std::memcpy(buffer->data.get(), frame_->data[0], frame_size);

  buffer->isKeyFrame = false;  // Audio typically doesn't have keyframes
  mStatus.currentTimeUs = frame_->pts;
  mStatus.bufferedBytes = std::max(static_cast<ssize_t>(0), static_cast<ssize_t>(mStatus.bufferedBytes) - frame_size);
  return OK;
}

status_t FFmpegAudioDecoder::seek(int64_t time_us) {
  std::lock_guard<std::mutex> lock(mMutex);
  if (!initialized_) {
    return ERROR_INVALID_FORMAT;
  }

  flush();
  mStatus.currentTimeUs = time_us;
  return OK;
}

status_t FFmpegAudioDecoder::flush() {
  std::lock_guard<std::mutex> lock(mMutex);
  if (!initialized_) {
    return OK;
  }

  avcodec_flush_buffers(codec_context_);
  mStatus.bufferedBytes = 0;
  mStatus.isDecoding = false;
  return OK;
}

void FFmpegAudioDecoder::release() {
  std::lock_guard<std::mutex> lock(mMutex);
  FreeResources();
  initialized_ = false;
  mInitialized = false;
}

status_t FFmpegAudioDecoder::onFormatChanged(const MetaData& new_meta) {
  release();
  return init(new_meta);
}

void FFmpegAudioDecoder::FreeResources() {
  if (frame_) {
    av_frame_free(&frame_);
  }
  if (packet_) {
    av_packet_free(&packet_);
  }
  if (codec_context_) {
    avcodec_free_context(&codec_context_);
  }
  codec_ = nullptr;
}

bool FFmpegAudioDecoder::isSupportedMime(const std::string& mime_type) {
  return FindCodecByMime(mime_type, false) != nullptr;
}

}  // namespace hpc
