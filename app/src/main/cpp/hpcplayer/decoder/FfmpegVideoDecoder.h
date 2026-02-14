#pragma once

#include "Decoder.h"

extern "C" {
#include "libavcodec/avcodec.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
}

class FfmpegVideoDecoder final : public Decoder {
public:
    explicit FfmpegVideoDecoder(const std::shared_ptr<Looper>& looper);
    ~FfmpegVideoDecoder() override;

protected:
    void onMessageReceived(const Message& msg) override;

private:
    void doConfigure(const std::shared_ptr<MediaFormat>& format);
    void doSetRenderer(const std::shared_ptr<Renderer>& renderer);
    void doSetExtractor(const std::shared_ptr<Extractor>& extractor);
    void doStart();
    void doStop();
    void doFlush();
    void doRequestInputBuffers();

    AVCodecContext* codec_context_ = nullptr;
    AVFrame* av_frame_ = nullptr;
    SwsContext* sws_context_ = nullptr;
    int target_width_ = 0;
    int target_height_ = 0;
    AVPixelFormat target_pixel_format_ = AV_PIX_FMT_RGBA;
};
