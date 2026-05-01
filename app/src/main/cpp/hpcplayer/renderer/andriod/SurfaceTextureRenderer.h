#ifndef HPC_PLAYER_RENDERER_ANDROID_SURFACE_TEXTURE_RENDERER_H_
#define HPC_PLAYER_RENDERER_ANDROID_SURFACE_TEXTURE_RENDERER_H_

#include "../Renderer.h"
#include "EglEnv.h"
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <jni.h>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>

namespace hpc {

class NativeWindow;

/**
 * SurfaceTextureRenderer
 *
 * 实现 SurfaceTexture + OES 纹理 + EGL 渲染的渲染器
 * 所有 OpenGL ES 和 EGL 操作在专用渲染线程中执行
 */
class SurfaceTextureRenderer final : public Renderer {
public:
    SurfaceTextureRenderer();
    ~SurfaceTextureRenderer() override;

    void init() override;
    void stop() override;
    std::shared_ptr<NativeWindow> setOutputWindow(const std::shared_ptr<NativeWindow>& window);

protected:
    void onMessageReceived(const Message& msg) override;

private:
    // Rendering thread operations
    void renderThreadMain();
    void doRender(const std::shared_ptr<MediaFrame>& frame);
    void notifyConsume();
    void doFlush();
    void doPause();
    void doResume();
    void doStop();
    void shutdownRenderThread();

    // OpenGL ES operations
    void setupShaders();
    void cleanupShaders();
    void setupTextures();
    void cleanupTextures();
    bool createCodecSurfaceTexture();
    void releaseCodecSurfaceTexture();
    bool updateSurfaceTexture();
    void drawFrame();
    void updateTransformMatrix();

    // Message types
    enum {
        kWhatStop              = 'stop'
    };

    // EGL and OpenGL state
    std::unique_ptr<EglEnv> mEglEnv;
    std::thread mRenderThread;
    std::mutex mRenderLock;
    std::condition_variable mRenderCond;
    std::atomic<bool> mRenderThreadRunning{false};

    // OpenGL ES program and attributes
    GLuint mProgram = 0;
    GLint mPositionHandle = -1;
    GLint mTexCoordHandle = -1;
    GLint mMatrixHandle = -1;
    GLint mTextureHandle = -1;

    // Textures
    GLuint mOESTextureId = 0;
    GLuint mVertexArrayObject = 0;
    GLuint mVertexBuffer = 0;
    GLuint mTexCoordBuffer = 0;

    std::shared_ptr<NativeWindow> mOutputWindow;
    std::shared_ptr<NativeWindow> mCodecWindow;
    jobject mSurfaceTexture = nullptr;
    jobject mSurface = nullptr;

    std::mutex mInitLock;
    std::condition_variable mInitCond;
    bool mInitFinished = false;
    bool mInitSucceeded = false;

    // Frame dimensions
    int mVideoWidth = 0;
    int mVideoHeight = 0;

    // Transform matrix
    float mTransformMatrix[16];

    // State flags
    bool mFramePrepared = false;
    std::shared_ptr<MediaFrame> mCurrentFrame;
};

} // namespace hpc

#endif // HPC_PLAYER_RENDERER_ANDROID_SURFACE_TEXTURE_RENDERER_H_

