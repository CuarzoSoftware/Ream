#include <CZ/Ream/RCore.h>
#include <CZ/Ream/GL/RGLMakeCurrent.h>
#include <CZ/Ream/GL/RGLDevice.h>
#include <CZ/Ream/RLog.h>
#include <cstring>
#include <GLES2/gl2.h>

using namespace CZ;

// See Bind(). With a surface current
static void SelectBackBuffer() noexcept
{
    using DrawBuffersProc = void (*)(GLsizei, const GLenum *);
    using ReadBufferProc = void (*)(GLenum);
    static const auto drawBuffers { reinterpret_cast<DrawBuffersProc>(eglGetProcAddress("glDrawBuffers")) };
    static const auto readBuffer { reinterpret_cast<ReadBufferProc>(eglGetProcAddress("glReadBuffer")) };

    // OpenGL ES 2 has no draw buffer selection (and the procs may still resolve)
    const char *version { reinterpret_cast<const char*>(glGetString(GL_VERSION)) };

    if (!drawBuffers || !readBuffer || !version || strncmp(version, "OpenGL ES ", 10) != 0 || version[10] < '3')
        return;

    GLint bound { 0 };
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &bound);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    const GLenum back { GL_BACK };
    drawBuffers(1, &back);
    readBuffer(GL_BACK);
    glBindFramebuffer(GL_FRAMEBUFFER, GLuint(bound));
}

EGLBoolean RGLMakeCurrent::Bind(EGLDisplay display, EGLSurface draw, EGLSurface read, EGLContext context) noexcept
{
    const EGLSurface prevDraw { eglGetCurrentSurface(EGL_DRAW) };
    const EGLContext prevContext { eglGetCurrentContext() };
    const EGLBoolean ok { eglMakeCurrent(display, draw, read, context) };

    // Only when going from no surface (or another context) to a surface: a few times per frame
    if (ok && draw != EGL_NO_SURFACE && context != EGL_NO_CONTEXT && (prevDraw == EGL_NO_SURFACE || prevContext != context))
        SelectBackBuffer();

    return ok;
}

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
        Bind(prevDisplay, prevDraw, prevRead, prevContext);
    return true;
}
