#ifndef RGLSTRINGS_H
#define RGLSTRINGS_H

#include <CZ/Ream/Ream.h>
#include <EGL/eglplatform.h>

class CZ::RGLStrings
{
public:
    static const char *EGLError(EGLint error) noexcept;
};

#endif // RGLSTRINGS_H
