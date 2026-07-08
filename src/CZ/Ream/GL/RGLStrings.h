#ifndef RGLSTRINGS_H
#define RGLSTRINGS_H

#include <CZ/Ream/Ream.h>
#include <EGL/eglplatform.h>

/**
 * @brief OpenGL/EGL string helpers.
 *
 * Utilities for converting EGL enums into human-readable strings, mainly used for logging.
 */
class CZ::RGLStrings
{
public:
    /**
     * @brief Returns a human-readable name for an EGL error code.
     *
     * @param error An EGL error code as returned by `eglGetError()`.
     * @return A static, null-terminated string with the error's name (e.g. "EGL_BAD_DISPLAY"),
     *         or "unknown error" if the code is not recognized.
     */
    static const char *EGLError(EGLint error) noexcept;
};

#endif // RGLSTRINGS_H
