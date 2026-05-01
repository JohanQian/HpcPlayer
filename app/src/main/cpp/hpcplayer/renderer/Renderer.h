#ifndef HPC_PLAYER_RENDERER_RENDERER_H_
#define HPC_PLAYER_RENDERER_RENDERER_H_

#include "common/Handler.h"
#include "common/DataQueue.h"
#include <memory>
#include <atomic>

namespace hpc {

class Decoder;
class MediaFrame;
class MediaClock;
class MediaFormat;

class Renderer : public Handler {
public:
    explicit Renderer(std::string name);
    ~Renderer() override;

    virtual void init() {};
    void setDecoder(const std::shared_ptr<Decoder>& decoder);
    void setMediaClock(const std::shared_ptr<MediaClock>& clock);
    void setMediaFormat(const std::shared_ptr<MediaFormat>& format) {mediaFormat = format;}
    void queueBuffer(const std::shared_ptr<MediaFrame>);
    void start();
    virtual void stop();
    void flush();
    void pause();
    void resume();

    virtual int64_t getCurrentPosition() { return -1; }

protected:
    void onMessageReceived(const Message& msg) override = 0;
    
    // Returns true if the frame should be rendered, false if it should be dropped.
    // This method may block (sleep) if the frame is too early.
    bool syncFrame(const std::shared_ptr<MediaFrame>& frame);

    enum {
        kWhatDrainAudioQueue     = 'draA',
        kWhatDrainVideoQueue     = 'draV',
        kWhatQueueBuffer         = 'queB',
        kWhatQueueEOS            = 'qEOS',
        kWhatFlush               = 'flus',
        kWhatPause               = 'paus',
        kWhatResume              = 'resm',
        kWhatSetDecoder          = 'setD',
        kWhatSetMediaClock       = 'setC',
        kWhatConsume             = 'conS'
    };

    std::weak_ptr<Decoder> decoder;
    std::shared_ptr<MediaClock> mediaClock;
    std::atomic<bool> isPaused{true};
    std::atomic<bool> isFlushing{false};
    std::atomic<bool> firstFrameAfterFlush{true};
    std::shared_ptr<MediaFormat> mediaFormat;
    DataQueue<std::shared_ptr<MediaFrame>> frameQueue {INT32_MAX};

    std::atomic<bool> isFirstFrame{true};
};

} // namespace hpc

#endif // HPC_PLAYER_RENDERER_RENDERER_H_
