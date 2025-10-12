#include <CZ/Ream/RCore.h>
#include <CZ/Ream/GL/RGLMakeCurrent.h>
#include <CZ/Ream/GL/RGLDevice.h>
#include <CZ/Ream/RLog.h>

using namespace CZ;

RGLMakeCurrent RGLMakeCurrent::FromDevice(RGLDevice *device, bool keepSurfaces) noexcept
{
    if (!device || !device->asGL())
    {
        RLog(CZError, CZLN, "Invalid RGLDevice");
        return { EGL_NO_DISPLAY, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT };
    }

    auto ctx { device->eglContext() };

    if (keepSurfaces && eglGetCurrentDisplay() == device->eglDisplay() && ctx != EGL_NO_CONTEXT)
        return { device->eglDisplay(), eglGetCurrentSurface(EGL_DRAW), eglGetCurrentSurface(EGL_READ), ctx };
    else
        return { device->eglDisplay(), EGL_NO_SURFACE, EGL_NO_SURFACE, ctx };
}

bool RGLMakeCurrent::restore() noexcept
{
    if (restored) return false;
    restored = true;
    if (prevDisplay != EGL_NO_DISPLAY)
        eglMakeCurrent(prevDisplay, prevDraw, prevRead, prevContext);
    return true;
}
