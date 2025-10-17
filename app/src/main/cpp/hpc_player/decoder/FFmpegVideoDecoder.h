#pragma once

#include <memory>
#include <mutex>
#include <string>

#include "DecoderBase.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/frame.h>
#include <libavutil/mem.h>
}

namespace hpc {

class FFmpegVideoDecoder : public Decoder {
 public:
  explicit FFmpegVideoDecoder(bool async_mode = true);
  ~FFmpegVideoDecoder() override;

  status_t init(const MetaData& meta) override;
  status_t input(const std::shared_ptr<MediaBuffer>& buffer) override;
  status_t output(std::shared_ptr<MediaBuffer>& buffer) override;
  status_t seek(int64_t time_us) override;
  status_t flush() override;
  void release() override;

  static bool isSupportedMime(const std::string& mime_type);

 private:
  status_t onFormatChanged(const MetaData& new_meta) override;
  void FreeResources();

  AVCodec* codec_ = nullptr;
  AVCodecContext* codec_context_ = nullptr;
  AVFrame* frame_ = nullptr;
  AVPacket* packet_ = nullptr;
  bool initialized_ = false;
};

class FFmpegAudioDecoder : public Decoder {
 public:
  explicit FFmpegAudioDecoder(bool async_mode = true);
  ~FFmpegAudioDecoder() override;

  status_t init(const MetaData& meta) override;
  status_t input(const std::shared_ptr<MediaBuffer>& buffer) override;
  status_t output(std::shared_ptr<MediaBuffer>& buffer) override;
  status_t seek(int64_t time_us) override;
  status_t flush() override;
  void release() override;

  static bool isSupportedMime(const std::string& mime_type);

 private:
  status_t onFormatChanged(const MetaData& new_meta) override;
  void FreeResources();

  AVCodec* codec_ = nullptr;
  AVCodecContext* codec_context_ = nullptr;
  AVFrame* frame_ = nullptr;
  AVPacket* packet_ = nullptr;
  bool initialized_ = false;
};

}  // namespace hpc
