#include "FFmpegExtractor.h"
#include "Log.h"
#include "MetaData.h"

#define LOG_TAG "FFmpegExtractor"

namespace hpc {
status_t FFmpegExtractor::init(const char *url) {
  ALOGD("init");

  int ret = avformat_open_input(&mFormatContext, url, NULL, NULL);

  if (ret < 0) {
    release();
  }

  avformat_find_stream_info(mFormatContext, NULL);
  mVideoStream = av_find_best_stream(mFormatContext, AVMEDIA_TYPE_VIDEO, -1, -1, NULL,0);
  mAudioStream = av_find_best_stream(mFormatContext, AVMEDIA_TYPE_AUDIO, -1, -1, NULL,0);

  mCodecParam = mFormatContext->streams[mVideoStream]->codecpar;
  switch (mCodecParam->codec_id) {
//        case AV_CODEC_ID_WMV2:
//        case AV_CODEC_ID_WMV1:
//        case AV_CODEC_ID_WMV3:
//          return FAILED;
    case AV_CODEC_ID_HEVC:
      mMetaData->mime = "video/hevc";
      break;
    case AV_CODEC_ID_H264:
      mMetaData->mime = "video/avc";
      break;
    case AV_CODEC_ID_VP8:
      mMetaData->mime = "video/x-vnd.on2.vp8";
      break;
    case AV_CODEC_ID_VP9:
      mMetaData->mime = "video/x-vnd.on2.vp9";
      break;
    case AV_CODEC_ID_MPEG4:
      mMetaData->mime = "video/mp4v-es";
      break;
    case AV_CODEC_ID_MJPEG:
      mMetaData->mime = "video/x-motion-jpeg";
      break;
    case AV_CODEC_ID_MPEG1VIDEO:
    case AV_CODEC_ID_MPEG2VIDEO:
      mMetaData->mime = "video/mpeg2";
      break;
    default:
  }

  mMetaData->width = mCodecParam->width;
  mMetaData->height = mCodecParam->height;
  mMetaData->bitsPerSample = mFormatContext->streams[mAudioStream]->codecpar->sample_rate;
  mMetaData->channelCount = mFormatContext->streams[mAudioStream]->codecpar->channels;

  ALOGD("init end");
}

void FFmpegExtractor::release() {
  if (mFormatContext) {
    avformat_close_input(&mFormatContext);
    avformat_free_context(mFormatContext);
    mFormatContext = nullptr;
  }
}

int FFmpegExtractor::read(std::unique_ptr<MediaPacket> &packet, int index) {
  int ret = av_read_frame(mFormatContext,mAVPacket);
  return ret;
}

void FFmpegExtractor::getMetaData(MetaData &meta) {
  meta = *mMetaData;
}

status_t FFmpegExtractor::seek(int64_t position) {
  int ret = avformat_seek_file(mFormatContext,-1,INT64_MIN,position,INT64_MAX,AVSEEK_FLAG_BACKWARD | AVSEEK_FLAG_FRAME);
  if (ret < 0) {
    return ERROR;
  }
  return OK;
}

} // hpc