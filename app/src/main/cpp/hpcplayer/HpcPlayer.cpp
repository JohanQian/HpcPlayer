#include "HpcPlayer.h"
#include "common/HpcMessage.h"
#include "common/HpcLog.h"

namespace hpc {

namespace {
    constexpr char kTag[] = "HpcPlayer";
}

std::shared_ptr<HpcPlayer> HpcPlayer::create() {
    auto player = std::shared_ptr<HpcPlayer>(new HpcPlayer());
    player->init();
    return player;
}

HpcPlayer::HpcPlayer() : state(kStateIdle) {
}

void HpcPlayer::init() {
    core = HpcCore::create(weak_from_this());
    core->setMessageCallback([this](const Message& msg) {
        onMessage(msg);
    });
}

HpcPlayer::~HpcPlayer() {
    if (core) {
        core->release();
    }
}

void HpcPlayer::setDataSource(const char* path) {
    {
        std::lock_guard<std::mutex> lk(lock);
        if (state != kStateIdle) {
            return;
        }
        state = kStateSetDataSourcePending;
    }
    if (core) {
        core->setDataSource(path);
    }
}

void HpcPlayer::setSurface(std::shared_ptr<NativeWindow> window, VideoRenderMode mode) {
    std::lock_guard<std::mutex> lk(lock);
    if (core) {
        core->setSurface(window, mode);
    }
}

void HpcPlayer::prepareAsync() {
    std::unique_lock<std::mutex> lk(lock);
    switch (state) {
        case kStateUnprepared:
            state = kStatePreparing;
            isAsyncPrepare = false;
            if (core) {
                core->prepare();
            }
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
            if (core) {
                core->start();
            }
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
    if (core) {
        core->resume();
    }
    state = kStateRunning;
}

void HpcPlayer::pause() {
    std::lock_guard<std::mutex> lk(lock);
    if (state != kStateRunning) {
        return;
    }
    if (core) {
        core->pause();
    }
    state = kStatePaused;
}

void HpcPlayer::stop() {
    std::lock_guard<std::mutex> lk(lock);
    if (state != kStateRunning && state != kStatePaused && state != kStatePrepared) {
        return;
    }
    if (core) {
        core->stop();
    }
    state = kStateStopped;
}

void HpcPlayer::seekTo(long msec) {
    std::lock_guard<std::mutex> lk(lock);
    if (core) {
        core->seekTo(msec);
    }
    if (state == kStatePaused) {
        state = kStateRunning;
    }
}

void HpcPlayer::setLooping(bool looping) {
    std::lock_guard<std::mutex> lk(lock);
    this->looping = looping;
}

int64_t HpcPlayer::getCurrentPosition() {
    if (core) {
        return core->getCurrentPosition();
    }
    return 0;
}

int64_t HpcPlayer::getDuration() {
    if (core) {
        return core->getDuration();
    }
    return 0;
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
            // Handled via direct call from HpcCore now, but keeping for compatibility if needed
            // status_t err = static_cast<status_t>(msg.arg1);
            // notifyPrepareCompleted(err);
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
                    if (core) {
                        core->seekTo(0);
                    }
                    return;
                }
                
                if (core) {
                    core->pause();
                }
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

} // namespace hpc
