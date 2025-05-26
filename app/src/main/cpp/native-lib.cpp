#include <jni.h>
#include <string>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include "hpc_player/HpcPlayer.h"

using namespace hpc;

extern "C" {

HpcPlayer* getPlayer(JNIEnv *env, jobject thiz) {
    jclass clazz = env->GetObjectClass(thiz);
    jfieldID fieldId = env->GetFieldID(clazz, "nativePlayerPtr", "J");
    return reinterpret_cast<HpcPlayer*>(env->GetLongField(thiz, fieldId));
}

JNIEXPORT void JNICALL
Java_com_example_hpcplayer_HpcPlayer_nativeInit(JNIEnv *env, jobject thiz) {
    auto *player = new HpcPlayer();
    jclass clazz = env->GetObjectClass(thiz);
    jfieldID fieldId = env->GetFieldID(clazz, "nativePlayerPtr", "J");
    env->SetLongField(thiz, fieldId, reinterpret_cast<jlong>(player));
}

JNIEXPORT void JNICALL
Java_com_example_hpcplayer_HpcPlayer_nativeSetDataSource(
        JNIEnv *env, jobject thiz, jstring path) {
    auto *player = getPlayer(env, thiz);
    const char *cPath = env->GetStringUTFChars(path, nullptr);
    player->setDataSource(cPath);
    env->ReleaseStringUTFChars(path, cPath);
}

JNIEXPORT void JNICALL
Java_com_example_hpcplayer_HpcPlayer_nativeSetSurface(
        JNIEnv *env, jobject thiz, jobject surface) {
    auto *player = getPlayer(env, thiz);
    ANativeWindow *window = ANativeWindow_fromSurface(env, surface);
    player->setSurface(reinterpret_cast<NativeWindow *>(window));
}

JNIEXPORT void JNICALL
Java_com_example_hpcplayer_HpcPlayer_nativePrepare(JNIEnv *env, jobject thiz) {
    auto *player = getPlayer(env, thiz);
    player->prepare();
}

JNIEXPORT void JNICALL
Java_com_example_hpcplayer_HpcPlayer_nativeStart(JNIEnv *env, jobject thiz) {
    auto *player = getPlayer(env, thiz);
    player->start();
}

JNIEXPORT void JNICALL
Java_com_example_hpcplayer_HpcPlayer_nativePause(JNIEnv *env, jobject thiz) {
    auto *player = getPlayer(env, thiz);
    player->pause();
}

JNIEXPORT void JNICALL
Java_com_example_hpcplayer_HpcPlayer_nativeStop(JNIEnv *env, jobject thiz) {
    auto *player = getPlayer(env, thiz);
    player->stop();
}

JNIEXPORT void JNICALL
Java_com_example_hpcplayer_HpcPlayer_nativeSeekTo(
        JNIEnv *env, jobject thiz, jlong position) {
    auto *player = getPlayer(env, thiz);
    player->seekTo(static_cast<long>(position));
}

JNIEXPORT jlong JNICALL
Java_com_example_hpcplayer_HpcPlayer_nativeGetCurrentPosition(
        JNIEnv *env, jobject thiz) {
    auto *player = getPlayer(env, thiz);
    return player->getCurrentPosition();
}

JNIEXPORT jlong JNICALL
Java_com_example_hpcplayer_HpcPlayer_nativeGetDuration(
        JNIEnv *env, jobject thiz) {
    auto *player = getPlayer(env, thiz);
    return player->getDuration();
}

JNIEXPORT jboolean JNICALL
Java_com_example_hpcplayer_HpcPlayer_nativeIsPlaying(
        JNIEnv *env, jobject thiz) {
    auto *player = getPlayer(env, thiz);
    return player->isPlaying();
}

JNIEXPORT void JNICALL
Java_com_example_hpcplayer_HpcPlayer_nativeRelease(JNIEnv *env, jobject thiz) {
    auto *player = getPlayer(env, thiz);
    player->release();
    delete player;

    jclass clazz = env->GetObjectClass(thiz);
    jfieldID fieldId = env->GetFieldID(clazz, "nativePlayerPtr", "J");
    env->SetLongField(thiz, fieldId, 0);
}

}