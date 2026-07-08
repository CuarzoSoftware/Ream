#ifndef CZ_RGLIMAGEWRAP_H
#define CZ_RGLIMAGEWRAP_H

#include <CZ/Ream/RImageWrap.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <algorithm>

namespace CZ
{
    /**
     * @brief Maps a Ream RImageWrap addressing mode to its OpenGL ES equivalent.
     *
     * @param wrap The Ream wrapping mode. Values outside the valid range are clamped.
     * @return The matching GL wrap constant (`GL_REPEAT`, `GL_MIRRORED_REPEAT`,
     *         `GL_CLAMP_TO_EDGE`, or `GL_CLAMP_TO_BORDER_OES`).
     */
    inline GLenum RImageWrapToGL(RImageWrap wrap) noexcept
    {
        static const GLenum RGLImageWrap[4]
        {
            GL_REPEAT,
            GL_MIRRORED_REPEAT,
            GL_CLAMP_TO_EDGE,
            GL_CLAMP_TO_BORDER_OES
        };

        return RGLImageWrap[static_cast<int>(std::clamp(wrap, RImageWrap::Repeat, RImageWrap::ClampToBorder))];
    }
};

#endif // CZ_RGLIMAGEWRAP_H
