#ifndef RGLMAKECURRENT_H
#define RGLMAKECURRENT_H

#include <CZ/Ream/Ream.h>
#include <EGL/egl.h>

class CZ::RGLMakeCurrent
{
public:
    CZ_DISABLE_COPY(RGLMakeCurrent)

    static RGLMakeCurrent FromDevice(RGLDevice *device, bool keepSurfaces) noexcept;

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

    bool restore() noexcept
    {
        if (restored) return false;
        restored = true;
        eglMakeCurrent(prevDisplay, prevDraw, prevRead, prevContext);
        return true;
    }

private:
    EGLDisplay prevDisplay;
    EGLSurface prevDraw;
    EGLSurface prevRead;
    EGLContext prevContext;
    bool restored { false };
};

#endif // RGLMAKECURRENT_H
