//
// Created by qianjiang on 2026/3/28.
//

#ifndef HPC_GLUTILS_H
#define HPC_GLUTILS_H

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

namespace hpc {

class GLUtils {
public:
    static GLuint createOesTexture() {
        GLuint textureId;
        glGenTextures(1, &textureId);
        glBindTexture(GL_TEXTURE_EXTERNAL_OES, textureId);

        glTexParameterf(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameterf(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);
        return textureId;
    }
};

} // namespace hpc

#endif //HPC_GLUTILS_H
