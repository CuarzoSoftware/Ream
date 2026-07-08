#ifndef RGLTEXTURE_H
#define RGLTEXTURE_H

#include <GLES2/gl2.h>

namespace CZ
{
    /**
     * @brief A reference to an OpenGL ES texture object.
     *
     * Pairs a texture name with its binding target. The target is typically
     * `GL_TEXTURE_2D` for regular textures or `GL_TEXTURE_EXTERNAL_OES` for
     * textures backed by an EGLImage (e.g. imported DMA-buffers).
     */
    struct RGLTexture
    {
        /// The OpenGL texture name (0 means no texture).
        GLuint id;

        /// The texture binding target (e.g. `GL_TEXTURE_2D` or `GL_TEXTURE_EXTERNAL_OES`).
        GLenum target;
    };
}


#endif // RGLTEXTURE_H
