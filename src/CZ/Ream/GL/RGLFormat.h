#ifndef CZ_RGLFORMAT_H
#define CZ_RGLFORMAT_H

#include <CZ/Ream/Ream.h>
#include <GLES2/gl2.h>

namespace CZ
{
    struct RGLFormat
    {
        GLint format;
        GLint internalFormat;
        GLint type;

        static const RGLFormat *FromDRM(RFormat format) noexcept;
    };
}

#endif // CZ_RGLFORMAT_H
