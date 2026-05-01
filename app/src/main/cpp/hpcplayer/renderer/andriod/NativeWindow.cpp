#include "NativeWindow.h"
namespace hpc {
    NativeWindow::NativeWindow(ANativeWindow *window)
    {
        mWindow = window;
    }

    NativeWindow::~NativeWindow()
    {
        if (mWindow != nullptr) {
            ANativeWindow_release(mWindow);
            mWindow = nullptr;
        }
    }

    ANativeWindow *NativeWindow::get()
    {
        return mWindow;
    }

    int NativeWindow::getWidth()
    {
        if (mWindow != nullptr) {
            return ANativeWindow_getWidth(mWindow);
        }

        return 0;
    }

    int NativeWindow::getHeight()
    {
        if (mWindow != nullptr) {
            return ANativeWindow_getHeight(mWindow);
        }

        return 0;
    }

}