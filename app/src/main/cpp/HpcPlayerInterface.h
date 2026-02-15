#pragma once

#include <memory>
#include <cstdint>
#include <mutex>
#include <android/native_window.h>

enum media_event_type {
    MEDIA_NOP               = 0, // interface test message
    MEDIA_PREPARED          = 1,
    MEDIA_PLAYBACK_COMPLETE = 2,
    MEDIA_SEEK_COMPLETE     = 4,
    MEDIA_STARTED           = 6,
    MEDIA_PAUSED            = 7,
    MEDIA_STOPPED           = 8,
    MEDIA_NOTIFY_TIME       = 98,
    MEDIA_ERROR             = 100,
    MEDIA_INFO              = 200
};

class HpcPlayerListener {
public:
    virtual ~HpcPlayerListener() = default;
    virtual void notify(int msg, int ext1, int ext2, const void *obj) = 0;
};

class HpcPlayerInterface {
public:
    HpcPlayerInterface() = default;
    virtual ~HpcPlayerInterface() = default;

    virtual void setDataSource(const char* path) = 0;
    virtual void setSurface(std::shared_ptr<ANativeWindow> window) = 0;
    virtual void prepareAsync() = 0;
    virtual void start() = 0;
    virtual void resume() = 0;
    virtual void pause() = 0;
    virtual void stop() = 0;
    virtual void seekTo(long msec) = 0;
    virtual int64_t getDuration() = 0;
    virtual int64_t getCurrentPosition() = 0;

    virtual void setListener(const std::shared_ptr<HpcPlayerListener>& listener);
    void notify(int msg, int ext1, int ext2, const void *obj = nullptr);

protected:
    std::shared_ptr<HpcPlayerListener> mListener;
    std::mutex mNotifyLock;
};
