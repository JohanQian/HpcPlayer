#pragma once

#include "../Decoder.h"
#include <media/NdkMediaCodec.h>
#include <memory>

namespace hpc {

class NativeWindow;
struct MediaSample;

class MediaCodecVideoDecoder final : public Decoder {
public:
    explicit MediaCodecVideoDecoder(const std::shared_ptr<Looper>& looper);
    ~MediaCodecVideoDecoder() override;

    void setNativeWindow(const std::shared_ptr<NativeWindow>& window);
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

    AMediaCodec* codec = nullptr;
    std::shared_ptr<NativeWindow> nativeWindow = nullptr;
    bool codecStarted = false;

    std::shared_ptr<MediaSample> pendingSample = nullptr;
    bool isInputEosQueued = false;
};

} // namespace hpc
