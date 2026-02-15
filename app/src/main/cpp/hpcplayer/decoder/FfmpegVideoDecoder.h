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

    AVCodecContext* codecContext = nullptr;
    AVFrame* avFrame = nullptr;
    SwsContext* swsContext = nullptr;
    int targetWidth = 0;
    int targetHeight = 0;
    AVPixelFormat targetPixelFormat = AV_PIX_FMT_RGBA;
};
