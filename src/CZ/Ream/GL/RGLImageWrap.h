#ifndef CZ_RGLIMAGEWRAP_H
#define CZ_RGLIMAGEWRAP_H

#include <CZ/Ream/RImageWrap.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <algorithm>

namespace CZ
{
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
