#ifndef RGLMAKECURRENT_H
#define RGLMAKECURRENT_H

#include <CZ/Ream/Ream.h>
#include <EGL/egl.h>

// Scoped eglMakeCurrent (restores the prev state on destruction)
class CZ::RGLMakeCurrent
{
public:
    CZ_DISABLE_COPY(RGLMakeCurrent)

    [[nodiscard]] static RGLMakeCurrent FromDevice(RGLDevice *device, bool keepSurfaces) noexcept;

    RGLMakeCurrent() noexcept :
        prevDisplay(eglGetCurrentDisplay()),
        prevDraw(eglGetCurrentSurface(EGL_DRAW)),
        prevRead(eglGetCurrentSurface(EGL_READ)),
        prevContext(eglGetCurrentContext())
    {}

    RGLMakeCurrent(EGLDisplay display, EGLSurface draw, EGLSurface read, EGLContext context) noexcept :
        prevDisplay(eglGetCurrentDisplay()),
        prevDraw(eglGetCurrentSurface(EGL_DRAW)),
        prevRead(eglGetCurrentSurface(EGL_READ)),
        prevContext(eglGetCurrentContext())
    {
        eglMakeCurrent(display, draw, read, context);
    }

    ~RGLMakeCurrent() noexcept
    {
        restore();
    }

    bool restore() noexcept;

private:
    EGLDisplay prevDisplay;
    EGLSurface prevDraw;
    EGLSurface prevRead;
    EGLContext prevContext;
    bool restored { false };
};

#endif // RGLMAKECURRENT_H
