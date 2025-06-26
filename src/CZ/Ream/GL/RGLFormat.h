#ifndef RGLFORMAT_H
#define RGLFORMAT_H

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


#endif // RGLFORMAT_H
