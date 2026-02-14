#pragma once

#include "../Decoder.h"
#include <media/NdkMediaCodec.h>

struct ANativeWindow;
struct MediaSample;

class MediaCodecVideoDecoder final : public Decoder {
public:
    explicit MediaCodecVideoDecoder(const std::shared_ptr<Looper>& looper);
    ~MediaCodecVideoDecoder() override;

    void setNativeWindow(const std::shared_ptr<ANativeWindow>& window);
    AMediaCodec* getCodec() const;

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

    AMediaCodec* codec_ = nullptr;
    std::shared_ptr<ANativeWindow> native_window_ = nullptr;
    bool codec_started_ = false;

    std::shared_ptr<MediaSample> pending_sample_ = nullptr;
    bool is_input_eos_queued_ = false;
};
