#ifndef HPC_PLAYER_HPC_PLAYER_H_
#define HPC_PLAYER_HPC_PLAYER_H_

#include <atomic>
#include <memory>
#include <string>
#include <string_view>
#include <mutex>
#include <condition_variable>
#include <functional>

#include "HpcCore.h"
#include "common/Message.h"
#include "../HpcPlayerInterface.h"

// Forward-declare ANativeWindow
struct ANativeWindow;

class HpcPlayer final : public HpcPlayerInterface, public std::enable_shared_from_this<HpcPlayer> {
public:
    static std::shared_ptr<HpcPlayer> create();
    ~HpcPlayer() override;

    void setDataSource(const char* path) override;
    void setSurface(std::shared_ptr<ANativeWindow> window) override;
    void prepareAsync() override;
    void start() override;
    void resume() override;
    void pause() override;
    void stop() override;
    void seekTo(long msec) override;
    int64_t getDuration() override;
    int64_t getCurrentPosition() override;
    
    // setListener is now implemented in base class, but we can override if needed.
    // For now, we use the base implementation.

    // Additional methods specific to HpcPlayer if needed
    void setLooping(bool looping);

    // Internal notification methods
    void notifyPrepareCompleted(status_t err);
    void notifySeekComplete();
    void notifyDuration(int64_t durationUs);
    void notifyPlaybackComplete();

private:
    HpcPlayer();
    void init();

    void onMessage(const Message& msg);
    void notifyListenerL(int msg, int ext1 = 0, int ext2 = 0);

    enum State {
        kStateIdle,
        kStateSetDataSourcePending,
        kStateUnprepared,
        kStatePreparing,
        kStatePrepared,
        kStateRunning,
        kStatePaused,
        kStateStopped,
        kStateResetInProgress
    };

    State state;
    std::mutex lock;
    std::condition_variable condition;
    status_t asyncResult = 0;
    bool isAsyncPrepare = false;
    bool autoLoop = false;
    bool looping = false;
    int64_t durationUs {0};

    std::shared_ptr<HpcCore> core;
    
    // mListener and mNotifyLock are now in base class
};

#endif // HPC_PLAYER_HPC_PLAYER_H_
