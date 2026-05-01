#include "SurfaceTextureRenderer.h"

#include "common/HpcLog.h"
#include "common/MediaClock.h"
#include "common/MediaFrame.h"
#include "common/Message.h"
#include "decoder/Decoder.h"
#include "renderer/andriod/NativeWindow.h"

#include <android/native_window_jni.h>
#include <jni.h>
#include <media/NdkMediaCodec.h>
#include <cstring>
#include <vector>

JavaVM* GetHpcJavaVm();
jobject HpcCreateSurfaceTexture(JNIEnv* env, jint textureId);
jobject HpcCreateSurface(JNIEnv* env, jobject surfaceTexture);

namespace hpc {

namespace {
constexpr char kTag[] = "SurfaceTextureRenderer";

const char* kVertexShader = R"(
    attribute vec4 a_position;
    attribute vec2 a_texCoord;
    uniform mat4 u_matrix;
    varying vec2 v_texCoord;
    void main() {
        gl_Position = a_position;
        vec2 texCoord = vec2(a_texCoord.x, 1.0 - a_texCoord.y);
        v_texCoord = (u_matrix * vec4(texCoord, 0.0, 1.0)).xy;
    }
)";

const char* kFragmentShader = R"(
    #extension GL_OES_EGL_image_external : require
    precision mediump float;
    uniform samplerExternalOES u_texture;
    varying vec2 v_texCoord;
    void main() {
        gl_FragColor = texture2D(u_texture, v_texCoord);
    }
)";

const GLfloat kVertices[] = {
        -1.0f, -1.0f, 0.0f,
         1.0f, -1.0f, 0.0f,
        -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f, 0.0f,
};

const GLfloat kTexCoords[] = {
        0.0f, 1.0f,
        1.0f, 1.0f,
        0.0f, 0.0f,
        1.0f, 0.0f,
};

GLuint compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint compiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled) {
        return shader;
    }

    GLint logLength = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
    if (logLength > 1) {
        std::vector<char> log(logLength);
        glGetShaderInfoLog(shader, logLength, nullptr, log.data());
        LOG_E("Shader compile failed: %s", log.data());
    }
    glDeleteShader(shader);
    return 0;
}

GLuint createProgram(const char* vertexSource, const char* fragmentSource) {
    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource);
    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource);
    if (!vertexShader || !fragmentShader) {
        if (vertexShader) glDeleteShader(vertexShader);
        if (fragmentShader) glDeleteShader(fragmentShader);
        return 0;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    GLint linked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    if (linked) {
        return program;
    }

    GLint logLength = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
    if (logLength > 1) {
        std::vector<char> log(logLength);
        glGetProgramInfoLog(program, logLength, nullptr, log.data());
        LOG_E("Program link failed: %s", log.data());
    }
    glDeleteProgram(program);
    return 0;
}

bool attachEnv(JNIEnv** env, bool* needsDetach) {
    JavaVM* vm = GetHpcJavaVm();
    if (!vm || !env || !needsDetach) {
        return false;
    }

    *needsDetach = false;
    if (vm->GetEnv(reinterpret_cast<void**>(env), JNI_VERSION_1_6) == JNI_OK) {
        return true;
    }

    if (vm->AttachCurrentThread(env, nullptr) == JNI_OK) {
        *needsDetach = true;
        return true;
    }
    return false;
}
} // namespace

SurfaceTextureRenderer::SurfaceTextureRenderer()
        : Renderer("SurfaceTextureRenderer") {
    updateTransformMatrix();
}

SurfaceTextureRenderer::~SurfaceTextureRenderer() {
    shutdownRenderThread();
}

void SurfaceTextureRenderer::init() {
}

void SurfaceTextureRenderer::stop() {
    isPaused = true;
    isFlushing = true;
    frameQueue.flush();
    sendMessageAndWait({kWhatFlush});
    {
        std::lock_guard<std::mutex> lock(mRenderLock);
        mFramePrepared = false;
        mCurrentFrame.reset();
    }
    shutdownRenderThread();
    isFlushing = false;
    firstFrameAfterFlush = true;
    isFirstFrame = true;
}

std::shared_ptr<NativeWindow> SurfaceTextureRenderer::setOutputWindow(
        const std::shared_ptr<NativeWindow>& window) {
    const bool wasPaused = isPaused.load();
    stop();
    isPaused = wasPaused;

    {
        std::lock_guard<std::mutex> lock(mInitLock);
        mOutputWindow = window;
        mCodecWindow.reset();
        mInitFinished = false;
        mInitSucceeded = false;
    }

    if (!window) {
        return nullptr;
    }

    mRenderThreadRunning = true;
    mRenderThread = std::thread(&SurfaceTextureRenderer::renderThreadMain, this);

    std::unique_lock<std::mutex> lock(mInitLock);
    mInitCond.wait(lock, [this] { return mInitFinished; });
    return mInitSucceeded ? mCodecWindow : nullptr;
}

void SurfaceTextureRenderer::onMessageReceived(const Message& msg) {
    switch (msg.what) {
        case kWhatSetDecoder:
            decoder = std::static_pointer_cast<Decoder>(msg.obj);
            break;
        case kWhatSetMediaClock:
            mediaClock = std::static_pointer_cast<MediaClock>(msg.obj);
            break;
        case kWhatResume:
            doResume();
            break;
        case kWhatPause:
            doPause();
            break;
        case kWhatFlush:
            doFlush();
            break;
        case kWhatConsume:
            notifyConsume();
            break;
        case kWhatStop:
            doStop();
            break;
        default:
            break;
    }
}

void SurfaceTextureRenderer::renderThreadMain() {
    mEglEnv = std::make_unique<EglEnv>();
    bool ok = mOutputWindow
              && mEglEnv->Init() == HpcStatus::kOk
              && mEglEnv->CreateSurface(mOutputWindow->get()) == HpcStatus::kOk
              && mEglEnv->MakeCurrent() == HpcStatus::kOk;

    if (ok) {
        setupShaders();
        setupTextures();
        ok = mProgram != 0 && mOESTextureId != 0 && createCodecSurfaceTexture();
    }

    {
        std::lock_guard<std::mutex> lock(mInitLock);
        mInitSucceeded = ok;
        mInitFinished = true;
    }
    mInitCond.notify_all();

    if (!ok) {
        mRenderThreadRunning = false;
    }

    while (mRenderThreadRunning) {
        std::unique_lock<std::mutex> lock(mRenderLock);
        mRenderCond.wait(lock, [this] { return mFramePrepared || !mRenderThreadRunning; });
        if (!mRenderThreadRunning) {
            break;
        }
        if (mFramePrepared) {
            if (updateSurfaceTexture()) {
                drawFrame();
                mEglEnv->SwapBuffers();
            }
            mFramePrepared = false;
        }
    }

    releaseCodecSurfaceTexture();
    cleanupTextures();
    cleanupShaders();
    if (mEglEnv) {
        mEglEnv->Release();
        mEglEnv.reset();
    }
}

void SurfaceTextureRenderer::notifyConsume() {
    if (isPaused || decoder.expired()) {
        return;
    }

    auto optFrame = frameQueue.tryPop();
    if (!optFrame) {
        sendMessageDelayed({kWhatConsume}, 5);
        return;
    }

    doRender(*optFrame);
    sendMessage({kWhatConsume});
}

void SurfaceTextureRenderer::doRender(const std::shared_ptr<MediaFrame>& frame) {
    if (!frame || !frame->codec || frame->index < 0) {
        return;
    }

    const bool render = syncFrame(frame);
    AMediaCodec_releaseOutputBuffer(frame->codec, frame->index, render);
    if (!render) {
        return;
    }

    std::lock_guard<std::mutex> lock(mRenderLock);
    mFramePrepared = true;
    mRenderCond.notify_one();
}

bool SurfaceTextureRenderer::updateSurfaceTexture() {
    if (!mSurfaceTexture) {
        return false;
    }

    JNIEnv* env = nullptr;
    bool needsDetach = false;
    if (!attachEnv(&env, &needsDetach)) {
        return false;
    }

    jclass surfaceTextureClass = env->GetObjectClass(mSurfaceTexture);
    jmethodID updateTexImage = env->GetMethodID(surfaceTextureClass, "updateTexImage", "()V");
    jmethodID getTransformMatrix = env->GetMethodID(surfaceTextureClass, "getTransformMatrix", "([F)V");

    env->CallVoidMethod(mSurfaceTexture, updateTexImage);
    if (!env->ExceptionCheck()) {
        jfloatArray matrix = env->NewFloatArray(16);
        env->CallVoidMethod(mSurfaceTexture, getTransformMatrix, matrix);
        env->GetFloatArrayRegion(matrix, 0, 16, mTransformMatrix);
        env->DeleteLocalRef(matrix);
    }

    bool ok = !env->ExceptionCheck();
    if (!ok) {
        env->ExceptionClear();
    }

    env->DeleteLocalRef(surfaceTextureClass);
    if (needsDetach) {
        GetHpcJavaVm()->DetachCurrentThread();
    }
    return ok;
}

void SurfaceTextureRenderer::doFlush() {
    frameQueue.flush();
    isFlushing = false;
    firstFrameAfterFlush = true;
    isFirstFrame = true;
}

void SurfaceTextureRenderer::doPause() {
    isPaused = true;
}

void SurfaceTextureRenderer::doResume() {
    isPaused = false;
    sendMessage({kWhatConsume});
}

void SurfaceTextureRenderer::doStop() {
    shutdownRenderThread();
}

void SurfaceTextureRenderer::shutdownRenderThread() {
    if (mRenderThreadRunning) {
        mRenderThreadRunning = false;
        mRenderCond.notify_all();
    }
    if (mRenderThread.joinable()) {
        mRenderThread.join();
    }
}

void SurfaceTextureRenderer::setupShaders() {
    mProgram = createProgram(kVertexShader, kFragmentShader);
    if (!mProgram) {
        return;
    }

    mPositionHandle = glGetAttribLocation(mProgram, "a_position");
    mTexCoordHandle = glGetAttribLocation(mProgram, "a_texCoord");
    mMatrixHandle = glGetUniformLocation(mProgram, "u_matrix");
    mTextureHandle = glGetUniformLocation(mProgram, "u_texture");
}

void SurfaceTextureRenderer::cleanupShaders() {
    if (mProgram) {
        glDeleteProgram(mProgram);
        mProgram = 0;
    }
}

void SurfaceTextureRenderer::setupTextures() {
    glGenTextures(1, &mOESTextureId);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, mOESTextureId);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);

    glGenBuffers(1, &mVertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, mVertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(kVertices), kVertices, GL_STATIC_DRAW);

    glGenBuffers(1, &mTexCoordBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, mTexCoordBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(kTexCoords), kTexCoords, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void SurfaceTextureRenderer::cleanupTextures() {
    if (mOESTextureId) {
        glDeleteTextures(1, &mOESTextureId);
        mOESTextureId = 0;
    }
    if (mVertexBuffer) {
        glDeleteBuffers(1, &mVertexBuffer);
        mVertexBuffer = 0;
    }
    if (mTexCoordBuffer) {
        glDeleteBuffers(1, &mTexCoordBuffer);
        mTexCoordBuffer = 0;
    }
}

bool SurfaceTextureRenderer::createCodecSurfaceTexture() {
    JNIEnv* env = nullptr;
    bool needsDetach = false;
    if (!attachEnv(&env, &needsDetach)) {
        return false;
    }

    jobject localSurfaceTexture = HpcCreateSurfaceTexture(env, static_cast<jint>(mOESTextureId));
    jobject localSurface = HpcCreateSurface(env, localSurfaceTexture);
    ANativeWindow* window = localSurface ? ANativeWindow_fromSurface(env, localSurface) : nullptr;

    if (localSurfaceTexture) {
        mSurfaceTexture = env->NewGlobalRef(localSurfaceTexture);
        env->DeleteLocalRef(localSurfaceTexture);
    }
    if (localSurface) {
        mSurface = env->NewGlobalRef(localSurface);
        env->DeleteLocalRef(localSurface);
    }
    if (window) {
        mCodecWindow = std::make_shared<NativeWindow>(window);
    }

    if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }
    if (needsDetach) {
        GetHpcJavaVm()->DetachCurrentThread();
    }
    return mSurfaceTexture && mSurface && mCodecWindow;
}

void SurfaceTextureRenderer::releaseCodecSurfaceTexture() {
    JNIEnv* env = nullptr;
    bool needsDetach = false;
    if (!attachEnv(&env, &needsDetach)) {
        return;
    }

    if (mSurfaceTexture) {
        jclass surfaceTextureClass = env->GetObjectClass(mSurfaceTexture);
        jmethodID release = env->GetMethodID(surfaceTextureClass, "release", "()V");
        env->CallVoidMethod(mSurfaceTexture, release);
        env->DeleteLocalRef(surfaceTextureClass);
        env->DeleteGlobalRef(mSurfaceTexture);
        mSurfaceTexture = nullptr;
    }
    if (mSurface) {
        jclass surfaceClass = env->GetObjectClass(mSurface);
        jmethodID release = env->GetMethodID(surfaceClass, "release", "()V");
        env->CallVoidMethod(mSurface, release);
        env->DeleteLocalRef(surfaceClass);
        env->DeleteGlobalRef(mSurface);
        mSurface = nullptr;
    }
    mCodecWindow.reset();

    if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }
    if (needsDetach) {
        GetHpcJavaVm()->DetachCurrentThread();
    }
}

void SurfaceTextureRenderer::drawFrame() {
    if (!mProgram || !mOESTextureId || mEglEnv->MakeCurrent() != HpcStatus::kOk) {
        return;
    }

    glViewport(0, 0, mOutputWindow ? mOutputWindow->getWidth() : 0, mOutputWindow ? mOutputWindow->getHeight() : 0);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(mProgram);

    glBindBuffer(GL_ARRAY_BUFFER, mVertexBuffer);
    glEnableVertexAttribArray(mPositionHandle);
    glVertexAttribPointer(mPositionHandle, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

    glBindBuffer(GL_ARRAY_BUFFER, mTexCoordBuffer);
    glEnableVertexAttribArray(mTexCoordHandle);
    glVertexAttribPointer(mTexCoordHandle, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

    glUniformMatrix4fv(mMatrixHandle, 1, GL_FALSE, mTransformMatrix);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, mOESTextureId);
    glUniform1i(mTextureHandle, 0);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glDisableVertexAttribArray(mPositionHandle);
    glDisableVertexAttribArray(mTexCoordHandle);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);
}

void SurfaceTextureRenderer::updateTransformMatrix() {
    memset(mTransformMatrix, 0, sizeof(mTransformMatrix));
    mTransformMatrix[0] = 1.0f;
    mTransformMatrix[5] = 1.0f;
    mTransformMatrix[10] = 1.0f;
    mTransformMatrix[15] = 1.0f;
}

} // namespace hpc
