#pragma once

#include <jni.h>

class JniEnv {
public:
    JniEnv(JavaVM* vm) : jvm_(vm), env_(nullptr), attached_to_thread_(false) {
        if (jvm_ == nullptr) {
            return;
        }
        int result = jvm_->GetEnv(reinterpret_cast<void**>(&env_), JNI_VERSION_1_6);
        if (result == JNI_EDETACHED) {
            if (jvm_->AttachCurrentThread(&env_, nullptr) == 0) {
                attached_to_thread_ = true;
            } else {
                env_ = nullptr; // Failed to attach
            }
        } else if (result != JNI_OK) {
            env_ = nullptr; // Some other error
        }
    }

    ~JniEnv() {
        if (attached_to_thread_) {
            jvm_->DetachCurrentThread();
        }
    }

    // Disallow copy and assign
    JniEnv(const JniEnv&) = delete;
    JniEnv& operator=(const JniEnv&) = delete;

    JNIEnv* get() const {
        return env_;
    }

    // Allow the object to be used like a pointer
    JNIEnv* operator->() const {
        return env_;
    }

    // Allow checking if the env is valid
    explicit operator bool() const {
        return env_ != nullptr;
    }

private:
    JavaVM* jvm_;
    JNIEnv* env_;
    bool attached_to_thread_;
};
