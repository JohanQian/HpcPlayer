//#include "OpenGLRenderer.h"
//#include "../common/HpcLog.h"
//
//#include <GLES2/gl2ext.h>
//#include <android/native_window.h>
//
//extern "C" {
//#include "libavutil/frame.h"
//}
//
//namespace {
//    constexpr char kTag[] = "OpenGLRenderer";
//
//    // Vertex shader: Just passes through texture coordinates and vertex positions
//    const char* kVertexShader = R"(
//        attribute vec4 a_position;
//        attribute vec2 a_texCoord;
//        varying vec2 v_texCoord;
//        void main() {
//            gl_Position = a_position;
//            v_texCoord = a_texCoord;
//        }
//    )";
//
//    // Fragment shader: Samples Y, U, V textures and converts to RGB
//    const char* kFragmentShader = R"(
//        precision mediump float;
//        varying vec2 v_texCoord;
//        uniform sampler2D y_texture;
//        uniform sampler2D u_texture;
//        uniform sampler2D v_texture;
//        void main() {
//            float y = texture2D(y_texture, v_texCoord).r;
//            float u = texture2D(u_texture, v_texCoord).r - 0.5;
//            float v = texture2D(v_texture, v_texCoord).r - 0.5;
//            float r = y + 1.402 * v;
//            float g = y - 0.344 * u - 0.714 * v;
//            float b = y + 1.772 * u;
//            gl_FragColor = vec4(r, g, b, 1.0);
//        }
//    )";
//
//} // namespace
//
//OpenGLRenderer::OpenGLRenderer() = default;
//
//OpenGLRenderer::~OpenGLRenderer() {
//    Release();
//}
//
//HpcStatus OpenGLRenderer::Init(const std::any& params) {
//    ANativeWindow* window = nullptr;
//    try {
//        window = std::any_cast<ANativeWindow*>(params);
//    } catch (const std::bad_any_cast& e) {
//        LOG_E("Init: Failed to cast params to ANativeWindow*.");
//        return HpcStatus::kBadType;
//    }
//
//    if (egl_env_.Init() != HpcStatus::kOk) return HpcStatus::kUnknownError;
//    if (egl_env_.CreateSurface(window) != HpcStatus::kOk) return HpcStatus::kUnknownError;
//    if (egl_env_.MakeCurrent() != HpcStatus::kOk) return HpcStatus::kUnknownError;
//
//    return SetupShaders();
//}
//
//HpcStatus OpenGLRenderer::Render(std::shared_ptr<MediaFrame> frame) {
//    auto ffmpeg_frame = std::dynamic_pointer_cast<FfmpegVideoFrame>(frame);
//    if (!ffmpeg_frame || !ffmpeg_frame->frame) {
//        LOG_E("Render: Received a frame that is not an FfmpegVideoFrame.");
//        return HpcStatus::kBadType;
//    }
//
//    if (SetupTextures(frame) != HpcStatus::kOk) {
//        return HpcStatus::kUnknownError;
//    }
//
//    Draw();
//
//    return egl_env_.SwapBuffers();
//}
//
//void OpenGLRenderer::Release() {
//    egl_env_.Release();
//    shader_.Release();
//    if (y_texture_) glDeleteTextures(1, &y_texture_);
//    if (u_texture_) glDeleteTextures(1, &u_texture_);
//    if (v_texture_) glDeleteTextures(1, &v_texture_);
//}
//
//HpcStatus OpenGLRenderer::SetupShaders() {
//    if (!shader_.Load(kVertexShader, kFragmentShader)) {
//        LOG_E("Failed to load shaders.");
//        return HpcStatus::kUnknownError;
//    }
//    shader_.Use();
//    y_sampler_location_ = glGetUniformLocation(shader_.GetProgram(), "y_texture");
//    u_sampler_location_ = glGetUniformLocation(shader_.GetProgram(), "u_texture");
//    v_sampler_location_ = glGetUniformLocation(shader_.GetProgram(), "v_texture");
//    return HpcStatus::kOk;
//}
//
//HpcStatus OpenGLRenderer::SetupTextures(std::shared_ptr<MediaFrame> frame) {
//    auto* av_frame = std::dynamic_pointer_cast<FfmpegVideoFrame>(frame)->frame.get();
//    if (frame_width_ != av_frame->width || frame_height_ != av_frame->height) {
//        frame_width_ = av_frame->width;
//        frame_height_ = av_frame->height;
//
//        if (y_texture_) glDeleteTextures(1, &y_texture_);
//        if (u_texture_) glDeleteTextures(1, &u_texture_);
//        if (v_texture_) glDeleteTextures(1, &v_texture_);
//
//        glGenTextures(1, &y_texture_);
//        glBindTexture(GL_TEXTURE_2D, y_texture_);
//        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, frame_width_, frame_height_, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, nullptr);
//        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
//        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
//
//        glGenTextures(1, &u_texture_);
//        glBindTexture(GL_TEXTURE_2D, u_texture_);
//        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, frame_width_ / 2, frame_height_ / 2, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, nullptr);
//        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
//        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
//
//        glGenTextures(1, &v_texture_);
//        glBindTexture(GL_TEXTURE_2D, v_texture_);
//        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, frame_width_ / 2, frame_height_ / 2, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, nullptr);
//        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
//        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
//    }
//
//    // Upload Y, U, V plane data to textures
//    glActiveTexture(GL_TEXTURE0);
//    glBindTexture(GL_TEXTURE_2D, y_texture_);
//    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, frame_width_, frame_height_, GL_LUMINANCE, GL_UNSIGNED_BYTE, av_frame->data[0]);
//
//    glActiveTexture(GL_TEXTURE1);
//    glBindTexture(GL_TEXTURE_2D, u_texture_);
//    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, frame_width_ / 2, frame_height_ / 2, GL_LUMINANCE, GL_UNSIGNED_BYTE, av_frame->data[1]);
//
//    glActiveTexture(GL_TEXTURE2);
//    glBindTexture(GL_TEXTURE_2D, v_texture_);
//    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, frame_width_ / 2, frame_height_ / 2, GL_LUMINANCE, GL_UNSIGNED_BYTE, av_frame->data[2]);
//
//    return HpcStatus::kOk;
//}
//
//void OpenGLRenderer::Draw() {
//    static const GLfloat vertices[] = { -1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f };
//    static const GLfloat texCoords[] = { 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f };
//
//    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, vertices);
//    glEnableVertexAttribArray(0);
//    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, texCoords);
//    glEnableVertexAttribArray(1);
//
//    glUniform1i(y_sampler_location_, 0);
//    glUniform1i(u_sampler_location_, 1);
//    glUniform1i(v_sampler_location_, 2);
//
//    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
//}
