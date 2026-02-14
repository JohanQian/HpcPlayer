#include "EglEnv.h"
#include "common/HpcLog.h"

namespace {
    constexpr char kTag[] = "EglEnv";
} // namespace

EglEnv::EglEnv() = default;

EglEnv::~EglEnv() {
    Release();
}

HpcStatus EglEnv::Init(EGLContext shared_context) {
    display_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display_ == EGL_NO_DISPLAY) {
        LOG_E("eglGetDisplay failed");
        return HpcStatus::kUnknownError;
    }

    if (!eglInitialize(display_, nullptr, nullptr)) {
        LOG_E("eglInitialize failed");
        return HpcStatus::kUnknownError;
    }

    const EGLint attribs[] = { EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_BLUE_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_RED_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_NONE };
    EGLint num_configs;
    if (!eglChooseConfig(display_, attribs, &config_, 1, &num_configs)) {
        LOG_E("eglChooseConfig failed");
        return HpcStatus::kUnknownError;
    }

    const EGLint context_attribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    context_ = eglCreateContext(display_, config_, shared_context, context_attribs);
    if (context_ == EGL_NO_CONTEXT) {
        LOG_E("eglCreateContext failed");
        return HpcStatus::kUnknownError;
    }

    return HpcStatus::kOk;
}

HpcStatus EglEnv::CreateSurface(ANativeWindow* window) {
    if (display_ == EGL_NO_DISPLAY || context_ == EGL_NO_CONTEXT || config_ == nullptr) {
        return HpcStatus::kNoInit;
    }
    ReleaseSurface(); // Release existing surface if any
    surface_ = eglCreateWindowSurface(display_, config_, window, nullptr);
    if (surface_ == EGL_NO_SURFACE) {
        LOG_E("eglCreateWindowSurface failed");
        return HpcStatus::kUnknownError;
    }
    return HpcStatus::kOk;
}

HpcStatus EglEnv::MakeCurrent() {
    if (display_ == EGL_NO_DISPLAY || context_ == EGL_NO_CONTEXT || surface_ == EGL_NO_SURFACE) {
        return HpcStatus::kNoInit;
    }
    if (!eglMakeCurrent(display_, surface_, surface_, context_)) {
        LOG_E("eglMakeCurrent failed with error: %d", eglGetError());
        return HpcStatus::kUnknownError;
    }
    return HpcStatus::kOk;
}

HpcStatus EglEnv::SwapBuffers() {
    if (display_ == EGL_NO_DISPLAY || surface_ == EGL_NO_SURFACE) {
        return HpcStatus::kNoInit;
    }
    if (!eglSwapBuffers(display_, surface_)) {
        LOG_E("eglSwapBuffers failed");
        return HpcStatus::kUnknownError;
    }
    return HpcStatus::kOk;
}

void EglEnv::ReleaseSurface() {
    if (display_ != EGL_NO_DISPLAY && surface_ != EGL_NO_SURFACE) {
        eglDestroySurface(display_, surface_);
        surface_ = EGL_NO_SURFACE;
    }
}

void EglEnv::Release() {
    if (display_ != EGL_NO_DISPLAY) {
        eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        ReleaseSurface();
        if (context_ != EGL_NO_CONTEXT) {
            eglDestroyContext(display_, context_);
            context_ = EGL_NO_CONTEXT;
        }
        eglTerminate(display_);
        display_ = EGL_NO_DISPLAY;
    }
}
