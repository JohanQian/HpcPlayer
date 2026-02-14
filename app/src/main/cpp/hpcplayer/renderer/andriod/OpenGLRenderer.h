//#pragma once
//
//#include "Renderer.h"
//#include "EglEnv.h"
//#include "Shader.h"
//
//#include <GLES2/gl2.h>
//
//class OpenGLRenderer final : public Renderer {
//public:
//    OpenGLRenderer();
//    ~OpenGLRenderer() override;
//
//    HpcStatus Init(const std::any& params) override;
//    HpcStatus Render(std::shared_ptr<MediaFrame> frame) override;
//    void Release() override;
//
//private:
//    HpcStatus SetupShaders();
//    HpcStatus SetupTextures(std::shared_ptr<MediaFrame> frame);
//    void Draw();
//
//    EglEnv egl_env_;
//    Shader shader_;
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
//};
