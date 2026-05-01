#ifndef MEDIA_PLAYER_TEST_APP_SRC_MAIN_CPP_RENDERER_ANDRIOD_EGLENV_H_
#define MEDIA_PLAYER_TEST_APP_SRC_MAIN_CPP_RENDERER_ANDRIOD_EGLENV_H_

#include <EGL/egl.h>

#include "common/HpcStatus.h"

struct ANativeWindow;

namespace hpc {

class EglEnv final {
public:
    EglEnv();
    ~EglEnv();

    // Disallow copy and assign
    EglEnv(const EglEnv&) = delete;
    EglEnv& operator=(const EglEnv&) = delete;

    HpcStatus Init(EGLContext shared_context = EGL_NO_CONTEXT);
    HpcStatus CreateSurface(ANativeWindow* window);
    HpcStatus MakeCurrent();
    HpcStatus SwapBuffers();
    void ReleaseSurface();
    void Release();

private:
    EGLDisplay display_ = EGL_NO_DISPLAY;
    EGLConfig config_ = nullptr;
    EGLContext context_ = EGL_NO_CONTEXT;
    EGLSurface surface_ = EGL_NO_SURFACE;
};

} // namespace hpc

#endif  // MEDIA_PLAYER_TEST_APP_SRC_MAIN_CPP_RENDERER_ANDRIOD_EGLENV_H_