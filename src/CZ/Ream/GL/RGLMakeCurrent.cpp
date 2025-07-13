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

    if (keepSurfaces)
        return { device->eglDisplay(), eglGetCurrentSurface(EGL_DRAW), eglGetCurrentSurface(EGL_READ), device->eglContext() };
    else
        return { device->eglDisplay(), EGL_NO_SURFACE, EGL_NO_SURFACE, device->eglContext() };
}
