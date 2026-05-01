#ifndef HPC_NATIVEWINDOW_H
#define HPC_NATIVEWINDOW_H

#include <android/native_window.h>
#include <android/native_window_jni.h>
namespace hpc
{

    class NativeWindow
    {
    public:
        explicit
        NativeWindow(ANativeWindow* window);
        ~NativeWindow();
        ANativeWindow* get();
        int getWidth();
        int getHeight();
    private:
        ANativeWindow* mWindow{nullptr};
    };

} // namespace cicada
#endif //NATIVE_WINDOW_H