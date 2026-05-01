#pragma once

#include "Decoder.h"

namespace hpc {

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

    AVCodecContext* codecContext = nullptr;
    AVFrame* avFrame = nullptr;
    SwrContext* swrContext = nullptr;
    int64_t targetChannelLayout = AV_CH_LAYOUT_STEREO;
    AVSampleFormat targetSampleFmt = AV_SAMPLE_FMT_S16;
    int targetSampleRate = 44100;
};

} // namespace hpc
