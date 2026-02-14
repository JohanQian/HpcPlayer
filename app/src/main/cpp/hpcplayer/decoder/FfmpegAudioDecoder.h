#pragma once

#include "Decoder.h"

extern "C" {
#include "libavcodec/avcodec.h"
#include "libswresample/swresample.h"
}

class FfmpegAudioDecoder final : public Decoder {
public:
    explicit FfmpegAudioDecoder(const std::shared_ptr<Looper>& looper);
    ~FfmpegAudioDecoder() override;

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
    SwrContext* swr_context_ = nullptr;
    int64_t target_channel_layout_ = AV_CH_LAYOUT_STEREO;
    AVSampleFormat target_sample_fmt_ = AV_SAMPLE_FMT_S16;
    int target_sample_rate_ = 44100;
};
