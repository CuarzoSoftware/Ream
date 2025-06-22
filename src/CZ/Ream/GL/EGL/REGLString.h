#ifndef REGLSTRING_H
#define REGLSTRING_H

#include <EGL/eglplatform.h>

class REGLString
{
public:
    static const char *Error(EGLint error) noexcept;
};

#endif // REGLSTRING_H
