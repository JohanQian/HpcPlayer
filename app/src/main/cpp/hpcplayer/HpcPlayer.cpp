#include "HpcPlayer.h"
#include "common/HpcMessage.h"
#include "common/HpcLog.h"

namespace {
    constexpr char kTag[] = "HpcPlayer";
}

HpcPlayer::HpcPlayer() : core(HpcCore::create()), state(kStateIdle) {
    core->setMessageCallback([this](const Message& msg) {
        onMessage(msg);
    });
}

HpcPlayer::~HpcPlayer() {
    core->release();
}

void HpcPlayer::setDataSource(const char* path) {
    {
        std::lock_guard<std::mutex> lk(lock);
        if (state != kStateIdle) {
            return;
        }
        state = kStateSetDataSourcePending;
    }
    core->setDataSource(path);
}

void HpcPlayer::setSurface(std::shared_ptr<ANativeWindow> window) {
    std::lock_guard<std::mutex> lk(lock);
    core->setSurface(window);
}

void HpcPlayer::prepareAsync() {
    std::unique_lock<std::mutex> lk(lock);
    switch (state) {
        case kStateUnprepared:
            state = kStatePreparing;
            isAsyncPrepare = false;
            core->prepare();
            while (state == kStatePreparing) {
                condition.wait(lk);
            }
            break;
        default:
            break;
    }
}

void HpcPlayer::start() {
    std::unique_lock<std::mutex> lk(lock);
    switch (state) {
        case kStateUnprepared:
        {
            prepareAsync();
            if (state != kStatePrepared) {
                return;
            }
            // Fallthrough intended
        }
        case kStatePrepared:
        case kStatePaused:
        case kStateStopped:
        {
            core->start();
            state = kStateRunning;
            break;
        }
        case kStateRunning:
            break;
        default:
            break;
    }
}

void HpcPlayer::resume() {
    std::lock_guard<std::mutex> lk(lock);
    if (state != kStatePaused) {
        return;
    }
    core->resume();
    state = kStateRunning;
}

void HpcPlayer::pause() {
    std::lock_guard<std::mutex> lk(lock);
    if (state != kStateRunning) {
        return;
    }
    core->pause();
    state = kStatePaused;
}

void HpcPlayer::stop() {
    std::lock_guard<std::mutex> lk(lock);
    if (state != kStateRunning && state != kStatePaused && state != kStatePrepared) {
        return;
    }
    core->stop();
    state = kStateStopped;
}

void HpcPlayer::seekTo(long msec) {
    std::lock_guard<std::mutex> lk(lock);
    core->seekTo(msec);
    if (state == kStatePaused) {
        state = kStateRunning;
    }
}

void HpcPlayer::setLooping(bool looping) {
    std::lock_guard<std::mutex> lk(lock);
    this->looping = looping;
}

int64_t HpcPlayer::getCurrentPosition() {
    return core->getCurrentPosition();
}

int64_t HpcPlayer::getDuration() {
    return core->getDuration();
}

void HpcPlayer::onMessage(const Message& msg) {
    switch (msg.what) {
        case MSG_SET_DATA_SOURCE_COMPLETED:
        {
            std::lock_guard<std::mutex> lk(lock);
            if (state == kStateSetDataSourcePending) {
                state = kStateUnprepared;
            }
            break;
        }
        case MSG_PREPARE_COMPLETED:
        {
            status_t err = static_cast<status_t>(msg.arg1);
            notifyPrepareCompleted(err);
            break;
        }
        case MSG_PLAYBACK_COMPLETED:
        {
            notifyPlaybackComplete();
            break;
        }
        default:
            break;
    }
}

void HpcPlayer::notifyPrepareCompleted(status_t err) {
    LOG_V("notifyPrepareCompleted %d", err);

    std::unique_lock<std::mutex> lk(lock);

    if (state != kStatePreparing) {
        if (state == kStateResetInProgress || state == kStateIdle) {
             return;
        }
    }
    
    if (state != kStatePreparing) {
        LOG_E("notifyPrepareCompleted called in invalid state: %d", state);
        return;
    }

    asyncResult = err;

    if (err == 0) { // OK
        state = kStatePrepared;
        notifyListenerL(MEDIA_PREPARED);
    } else {
        state = kStateUnprepared;
        notifyListenerL(MEDIA_ERROR,  err);
    }

    condition.notify_all();
}

void HpcPlayer::notifyPlaybackComplete() {
    std::unique_lock<std::mutex> lk(lock);
    notifyListenerL(MEDIA_PLAYBACK_COMPLETE);
}

void HpcPlayer::notifyListenerL(int msg, int ext1, int ext2) {
    LOG_V("notifyListenerL(%p), (%d, %d, %d)", this, msg, ext1, ext2);
    
    switch (msg) {
        case MEDIA_PLAYBACK_COMPLETE:
        {
            if (state != kStateResetInProgress) {
                if (looping || autoLoop) {
                    core->seekTo(0);
                    return;
                }
                
                core->pause();
                state = kStatePaused;
            }
            break;
        }
        default:
            break;
    }

    notify(msg, ext1, ext2);
}

void HpcPlayer::notifySeekComplete() {
    // Placeholder
}

void HpcPlayer::notifyDuration(int64_t durationUs) {
    this->durationUs = durationUs;
}
