#include <jni.h>
#include <string>
#include <android/native_window_jni.h>
#include <memory>

#include "hpcplayer/HpcPlayer.h"
#include "HpcPlayerInterface.h"

// Helper to cast the jlong back to a shared_ptr<HpcPlayer>
// The jlong is actually a pointer to a shared_ptr<HpcPlayer> on the heap.
inline std::shared_ptr<HpcPlayer> FromLong(jlong handle) {
    if (handle == 0) return nullptr;
    return *reinterpret_cast<std::shared_ptr<HpcPlayer>*>(handle);
}

// JNI Listener implementation
class JNIHpcPlayerListener : public HpcPlayerListener {
public:
    JNIHpcPlayerListener(JNIEnv* env, jobject thiz, jobject weak_thiz);
    ~JNIHpcPlayerListener();
    void notify(int msg, int ext1, int ext2, const void *obj) override;

private:
    JNIHpcPlayerListener();
    jclass      mClass;     // Reference to HpcNativePlayer class
    jobject     mObject;    // Weak ref to HpcNativePlayer Java object to call on
    JavaVM*     mJavaVM;    // Java VM to attach thread
};

JNIHpcPlayerListener::JNIHpcPlayerListener(JNIEnv* env, jobject thiz, jobject weak_thiz) {
    jclass clazz = env->GetObjectClass(thiz);
    if (clazz == NULL) {
        return;
    }
    mClass = (jclass)env->NewGlobalRef(clazz);
    mObject  = env->NewGlobalRef(weak_thiz);
    env->GetJavaVM(&mJavaVM);
}

JNIHpcPlayerListener::~JNIHpcPlayerListener() {
    JNIEnv *env = nullptr;
    if (mJavaVM->GetEnv((void**)&env, JNI_VERSION_1_4) == JNI_OK) {
        env->DeleteGlobalRef(mObject);
        env->DeleteGlobalRef(mClass);
    } else {
        if (mJavaVM->AttachCurrentThread(&env, NULL) == JNI_OK) {
            env->DeleteGlobalRef(mObject);
            env->DeleteGlobalRef(mClass);
            mJavaVM->DetachCurrentThread();
        }
    }
}

void JNIHpcPlayerListener::notify(int msg, int ext1, int ext2, const void *obj) {
    JNIEnv *env = nullptr;
    bool needsDetach = false;
    int ret = mJavaVM->GetEnv((void**)&env, JNI_VERSION_1_4);
    if (ret != JNI_OK) {
        ret = mJavaVM->AttachCurrentThread(&env, NULL);
        if (ret != JNI_OK) {
            return;
        }
        needsDetach = true;
    }

    jmethodID postEventId = env->GetMethodID(mClass, "onMessage", "(III)V");
    if (postEventId != NULL) {
        env->CallVoidMethod(mObject, postEventId, msg, ext1, ext2);
    }

    if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }

    if (needsDetach) {
        mJavaVM->DetachCurrentThread();
    }
}

extern "C" {

JNIEXPORT jlong JNICALL
Java_com_example_hpcplayer_HpcNativePlayer_nativeInit(JNIEnv* env, jobject thiz) {
    // Create HpcPlayer using create() which returns a shared_ptr
    auto player = HpcPlayer::create();
    // Store the shared_ptr on the heap so it persists
    auto* ptr = new std::shared_ptr<HpcPlayer>(player);
    return reinterpret_cast<jlong>(ptr);
}

JNIEXPORT void JNICALL
Java_com_example_hpcplayer_HpcNativePlayer_nativeSetupListener(JNIEnv* env, jobject thiz, jlong player_ptr) {
    if (auto player = FromLong(player_ptr)) {
        auto listener = std::make_shared<JNIHpcPlayerListener>(env, thiz, thiz);
        player->setListener(listener);
    }
}

JNIEXPORT void JNICALL
Java_com_example_hpcplayer_HpcNativePlayer_nativeRelease(JNIEnv* env, jobject thiz, jlong player_ptr) {
    auto* ptr = reinterpret_cast<std::shared_ptr<HpcPlayer>*>(player_ptr);
    if (ptr) {
        delete ptr; // This will decrement the ref count and potentially destroy the player
    }
}

JNIEXPORT void JNICALL
Java_com_example_hpcplayer_HpcNativePlayer_nativeSetDataSource(JNIEnv* env, jobject thiz, jlong player_ptr, jstring path) {
    if (auto player = FromLong(player_ptr)) {
        const char* c_path = env->GetStringUTFChars(path, nullptr);
        player->setDataSource(c_path);
        env->ReleaseStringUTFChars(path, c_path);
    }
}

JNIEXPORT void JNICALL
Java_com_example_hpcplayer_HpcNativePlayer_nativeSetSurface(JNIEnv* env, jobject thiz, jlong player_ptr, jobject surface) {
    if (auto player = FromLong(player_ptr)) {
        ANativeWindow* window = nullptr;
        if (surface != nullptr) {
            window = ANativeWindow_fromSurface(env, surface);
        }

        if (window) {
            player->setSurface(std::shared_ptr<ANativeWindow>(window, ANativeWindow_release));
        } else {
            player->setSurface(nullptr);
        }
    }
}

JNIEXPORT void JNICALL
Java_com_example_hpcplayer_HpcNativePlayer_nativePrepare(JNIEnv* env, jobject thiz, jlong player_ptr) {
    if (auto player = FromLong(player_ptr)) {
        player->prepareAsync();
    }
}

JNIEXPORT void JNICALL
Java_com_example_hpcplayer_HpcNativePlayer_nativeStart(JNIEnv* env, jobject thiz, jlong player_ptr) {
    if (auto player = FromLong(player_ptr)) {
        player->start();
    }
}

JNIEXPORT void JNICALL
Java_com_example_hpcplayer_HpcNativePlayer_nativeResume(JNIEnv* env, jobject thiz, jlong player_ptr) {
    if (auto player = FromLong(player_ptr)) {
        player->resume();
    }
}

JNIEXPORT void JNICALL
Java_com_example_hpcplayer_HpcNativePlayer_nativePause(JNIEnv* env, jobject thiz, jlong player_ptr) {
    if (auto player = FromLong(player_ptr)) {
        player->pause();
    }
}

JNIEXPORT void JNICALL
Java_com_example_hpcplayer_HpcNativePlayer_nativeStop(JNIEnv *env, jobject thiz, jlong player_ptr) {
    if (auto player = FromLong(player_ptr)) {
        player->stop();
    }
}

JNIEXPORT void JNICALL
Java_com_example_hpcplayer_HpcNativePlayer_nativeSeekTo(JNIEnv* env, jobject thiz, jlong player_ptr, jlong msec) {
    if (auto player = FromLong(player_ptr)) {
        player->seekTo(msec);
    }
}

JNIEXPORT jlong JNICALL
Java_com_example_hpcplayer_HpcNativePlayer_nativeGetDuration(JNIEnv* env, jobject thiz, jlong player_ptr) {
    if (auto player = FromLong(player_ptr)) {
        return player->getDuration();
    }
    return 0;
}

JNIEXPORT jlong JNICALL
Java_com_example_hpcplayer_HpcNativePlayer_nativeGetCurrentPosition(JNIEnv* env, jobject thiz, jlong player_ptr) {
    if (auto player = FromLong(player_ptr)) {
        return player->getCurrentPosition();
    }
    return 0;
}

} // extern "C"
