#include "HpcPlayerInterface.h"

// Assuming HpcLog.h defines ALOGV, ALOGE, ALOGW. If not, we define them here for this file.
#ifndef ALOGV
#include <android/log.h>
#define LOG_TAG "HpcPlayerInterface"
#define ALOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__)
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define ALOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#endif

namespace hpc {

void HpcPlayerInterface::setListener(const std::shared_ptr<HpcPlayerListener>& listener) {
    std::lock_guard<std::mutex> lock(mNotifyLock);
    mListener = listener;
}

void HpcPlayerInterface::notify(int msg, int ext1, int ext2, const void *obj) {
    ALOGV("message received msg=%d, ext1=%d, ext2=%d", msg, ext1, ext2);

    bool send = true;

    switch (msg) {
        case MEDIA_NOP:
            break;
        case MEDIA_PREPARED:
            ALOGV("HpcPlayerInterface::notify() prepared");
            break;
        case MEDIA_PLAYBACK_COMPLETE:
            ALOGV("playback complete");
            break;
        case MEDIA_ERROR:
            ALOGE("error (%d, %d)", ext1, ext2);
            break;
        case MEDIA_INFO:
            if (ext1 != 200) { 
                 ALOGW("info/warning (%d, %d)", ext1, ext2);
            }
            break;
        case MEDIA_SEEK_COMPLETE:
            ALOGV("Received seek complete");
            break;
        case MEDIA_STARTED:
            ALOGV("Received media started message");
            break;
        case MEDIA_NOTIFY_TIME:
            ALOGV("Received notify time message");
            break;
        default:
            ALOGV("unrecognized message: (%d, %d, %d)", msg, ext1, ext2);
            break;
    }

    std::shared_ptr<HpcPlayerListener> listener;
    {
        std::lock_guard<std::mutex> lock(mNotifyLock);
        listener = mListener;
    }

    if (listener && send) {
        ALOGV("callback application");
        listener->notify(msg, ext1, ext2, obj);
        ALOGV("back from callback");
    }
}

} // namespace hpc
