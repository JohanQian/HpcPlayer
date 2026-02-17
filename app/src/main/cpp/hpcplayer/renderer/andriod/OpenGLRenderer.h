//#pragma once
//
//#include "Renderer.h"
//#include "EglEnv.h"
//#include "Shader.h"
//#include "../common/Handler.h"
//#include "../common/MediaClock.h"
//#include "../common/SafeQueue.h"
//
//#include <GLES2/gl2.h>
//#include <thread>
//
//class OpenGLRenderer final : public Renderer, public Handler {
//public:
//    OpenGLRenderer();
//    ~OpenGLRenderer() override;
//
//    HpcStatus Init(const std::any& params) override;
//    HpcStatus Render(std::shared_ptr<MediaFrame> frame) override;
//    void Release() override;
//
//    void handle(const Message &msg) override;
//
//private:
//    void Run();
//    HpcStatus SetupShaders();
//    HpcStatus SetupTextures(std::shared_ptr<MediaFrame> frame);
//    void Draw();
//    void ReleaseGl();
//
//    enum {
//        kWhatQueueBuffer,
//    };
//
//    EglEnv egl_env_;
//    Shader shader_;
//    std::shared_ptr<MediaClock> clock_ = nullptr;
//    SafeQueue<std::shared_ptr<MediaFrame>> frame_queue_;
//
//    // YUV textures
//    GLuint y_texture_ = 0;
//    GLuint u_texture_ = 0;
//    GLuint v_texture_ = 0;
//
//    GLint y_sampler_location_ = -1;
//    GLint u_sampler_location_ = -1;
//    GLint v_sampler_location_ = -1;
//
//    int frame_width_ = 0;
//    int frame_height_ = 0;
//
//    ANativeWindow* window_ = nullptr;
//    std::thread thread_;
//    bool done_ = false;
//};
