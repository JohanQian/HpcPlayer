#pragma once

#include <android/native_window.h>

namespace hpc {

class Surface {
 public:
  explicit Surface(ANativeWindow *native_window) {
    if (native_window) {
      ANativeWindow_acquire(native_window);
      mNativeWindow = native_window;
    }
  }
  ~Surface() {
    if (mNativeWindow) {
      ANativeWindow_release(mNativeWindow);
      mNativeWindow = nullptr;
    }
  }
  ANativeWindow *get() { return mNativeWindow; }
  Surface(const Surface &) = delete;
  Surface &operator=(const Surface &) = delete;
  Surface(Surface &&) = delete;
  Surface &operator=(Surface &&) = delete;

 private:
  ANativeWindow *mNativeWindow{nullptr};
};

} // hpc

