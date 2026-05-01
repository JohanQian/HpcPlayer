#ifndef HPC_PLAYER_HPC_CORE_H_
#define HPC_PLAYER_HPC_CORE_H_

#include <atomic>
#include <functional>
#include <memory>
#include <string>

#include "common/HpcMessage.h"
#include "common/DataQueue.h"
#include "common/Clock.h"
#include "common/Handler.h"
#include "common/Looper.h"
#include "common/MediaSample.h"
#include "common/MediaFormat.h"
#include "decoder/Decoder.h"
#include "extractor/Extractor.h"
#include "renderer/Renderer.h"
#include "../HpcPlayerInterface.h"

struct ANativeWindow;

namespace hpc {

class MediaClock;
class HpcPlayer;

class NativeWindow;

struct PlayerConfig {
    bool useHardwareDecoding = true;
    bool useSystemClock = false;
};

class HpcCore : public Handler {
public:
    static std::shared_ptr<HpcCore> create(std::weak_ptr<HpcPlayer> player);
    ~HpcCore();

    void setMessageCallback(std::function<void(const Message&)> callback);

    void setDataSource(std::string_view path);
    void setSurface(const std::shared_ptr<NativeWindow>& window, VideoRenderMode mode = VideoRenderMode::SurfaceTextureOes);
    void prepare();
    void start();
    void resume();
    void pause();
    void stop();
    void seekTo(long msec);
    void release();

    int64_t getCurrentPosition();
    int64_t getDuration();

    enum {
        kWhatPrepare            = 'prep',
        kWhatStart              = 'strt',
        kWhatResume             = 'resm',
        kWhatPause              = 'paus',
        kWhatReset              = 'rset',
        kWhatSeek               = 'seek',
        kWhatSetVideoSurface    = '=VSu',
        kWhatMediaClockNotify   = 'mckN',
        kWhatSourceNotify       = 'srcN',
    };

private:
    explicit HpcCore(std::weak_ptr<HpcPlayer> player,std::shared_ptr<Looper> looper);
    void init();
    void initDecoder();
    void useVideoRenderer(VideoRenderMode mode);

    void onMessageReceived(const Message& msg) override;

    void onSourceNotify(const Message& msg);
    void notifyListener(const Message& msg);

    void doPrepare();
    void doStart();
    void doResume();
    void doPause();
    void doStop();
    void doSeekTo(long msec);
    void doRelease();

    PlayerConfig config;
    std::shared_ptr<Looper> looper;
    std::shared_ptr<Looper> videoLooper;
    std::shared_ptr<Looper> audioLooper;

    std::shared_ptr<Extractor> extractor;
    std::shared_ptr<Decoder> videoDecoder;
    std::shared_ptr<Decoder> audioDecoder;
    std::shared_ptr<Renderer> videoRenderer;
    std::shared_ptr<Renderer> audioRenderer;
    std::shared_ptr<MediaFormat> videoFormat;
    std::unique_ptr<Clock> clock;
    std::shared_ptr<MediaClock> mediaClock;

    int videoStreamIndex = -1;
    int audioStreamIndex = -1;

    std::function<void(const Message&)> messageCallback;
    std::mutex messageMutex;

    std::atomic_bool isRunning{false};
    std::atomic_bool isPlaying{false};
    
    std::weak_ptr<HpcPlayer> player;
};

} // namespace hpc

#endif // HPC_PLAYER_HPC_CORE_H_
