#ifndef HPC_PLAYER_RENDERER_ANDROID_MEDIA_CODEC_RENDERER_H_
#define HPC_PLAYER_RENDERER_ANDROID_MEDIA_CODEC_RENDERER_H_

#include "../Renderer.h"

class MediaCodecRenderer final : public Renderer {
public:
    MediaCodecRenderer();
    ~MediaCodecRenderer() override;

    void init();

protected:
    void onMessageReceived(const Message& msg) override;

private:
    void doSetDecoder(const std::shared_ptr<Decoder>& decoder);
    void doRender(const std::shared_ptr<MediaFrame>& frame);
    void doDrainQueue();
};

#endif // HPC_PLAYER_RENDERER_ANDROID_MEDIA_CODEC_RENDERER_H_
