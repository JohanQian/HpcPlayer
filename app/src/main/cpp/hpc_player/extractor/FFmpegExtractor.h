#pragma once

#include "Extractor.h"

extern "C" {
#include "libswscale/swscale.h"
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libswresample/swresample.h"
}

namespace hpc {

class MetaData;

class FFmpegExtractor : public Extractor{

  status_t init(const char *url) override;

  int read(std::unique_ptr<MediaPacket> &packet, int index) override;

  status_t seek(int64_t position) override;

  void getMetaData(MetaData& meta) override;

  void release() override;

 private:
  AVFormatContext* mFormatContext {nullptr};
  AVCodecParameters* mCodecParam {nullptr};
  AVPacket* mAVPacket {nullptr};
  std::shared_ptr<MetaData> mMetaData;
  int8_t mVideoStream {-1};
  int8_t mAudioStream {-1};
};

} // hpc
