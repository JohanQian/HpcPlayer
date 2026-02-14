#ifndef HPC_PLAYER_RENDERER_ANDROID_AUDIO_OPEN_SLES_RENDERER_H_
#define HPC_PLAYER_RENDERER_ANDROID_AUDIO_OPEN_SLES_RENDERER_H_

#include <atomic>
#include <mutex>
#include <queue>

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

#include "../Renderer.h"

class AudioOpenSLESRenderer final : public Renderer {
public:
    AudioOpenSLESRenderer();
    ~AudioOpenSLESRenderer() override;

    void init() override;
    int64_t getCurrentPosition() override;

protected:
    void onMessageReceived(const Message& msg) override;

private:
    void doSetDecoder(const std::shared_ptr<Decoder>& decoder);
    void doRender(const std::shared_ptr<MediaFrame>& frame);
    void doDrainQueue();
    void doFlush();
    void doPause();
    void doResume();
    void doRelease();
    void doEnqueue();
    bool ensurePlayerInitialized(int sampleRate, int channels);

    static void playerCallback(SLAndroidSimpleBufferQueueItf bq, void* context);

    SLObjectItf engineObject = nullptr;
    SLEngineItf engineEngine = nullptr;
    SLObjectItf outputMixObject = nullptr;
    SLObjectItf playerObject = nullptr;
    SLPlayItf playerPlay = nullptr;
    SLAndroidSimpleBufferQueueItf playerBufferQueue = nullptr;

    void createEngine();
    void createOutputMix();
    bool createPlayer(int sampleRate, int channels);
    
    int64_t currentPositionUs{0};
    std::mutex playerMutex;
    
    int64_t startPts{-1};
    
    // Keep track of the current frame being played to ensure data validity
    std::shared_ptr<MediaFrame> currentFrame;

    // Position tracking
    std::atomic<int64_t> playedBytes{0};
    int64_t bytesPerSecond = 0;
    std::queue<size_t> pendingBufferSizes;
    std::mutex queueMutex;
};

#endif // HPC_PLAYER_RENDERER_ANDROID_AUDIO_OPEN_SLES_RENDERER_H_
