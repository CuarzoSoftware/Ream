#ifndef RGLMAKECURRENT_H
#define RGLMAKECURRENT_H

#include <CZ/Ream/Ream.h>
#include <EGL/egl.h>

/**
 * @brief RAII guard for scoped `eglMakeCurrent()` calls.
 *
 * On construction it captures the currently bound EGL display, draw/read surfaces, and context,
 * then optionally binds a new context. On destruction (or an explicit restore()) it re-binds the
 * previously captured state. This is used throughout the GL backend to temporarily make a device's
 * context current on the calling thread without disturbing the caller's own binding.
 */
class CZ::RGLMakeCurrent
{
public:
    CZ_DISABLE_COPY(RGLMakeCurrent)

    /**
     * @brief Creates a guard that makes the given device's context current on the calling thread.
     *
     * @param device       The device whose per-thread EGL context should be made current.
     * @param keepSurfaces If `true` and the device's display is already current, the currently bound
     *                     draw/read surfaces are preserved; otherwise the context is bound surfaceless.
     * @return A guard bound to the device's context, or a no-op guard (EGL_NO_DISPLAY) if the device
     *         is invalid.
     */
    [[nodiscard]] static RGLMakeCurrent FromDevice(RGLDevice *device, bool keepSurfaces) noexcept;

    /**
     * @brief Captures the current EGL binding without changing it.
     *
     * Useful to snapshot and later restore() the current context.
     */
    RGLMakeCurrent() noexcept :
        prevDisplay(eglGetCurrentDisplay()),
        prevDraw(eglGetCurrentSurface(EGL_DRAW)),
        prevRead(eglGetCurrentSurface(EGL_READ)),
        prevContext(eglGetCurrentContext())
    {}

    /**
     * @brief Captures the current EGL binding and makes the given one current.
     *
     * @param display The EGL display to bind.
     * @param draw    The draw surface to bind.
     * @param read    The read surface to bind.
     * @param context The context to make current.
     */
    RGLMakeCurrent(EGLDisplay display, EGLSurface draw, EGLSurface read, EGLContext context) noexcept :
        prevDisplay(eglGetCurrentDisplay()),
        prevDraw(eglGetCurrentSurface(EGL_DRAW)),
        prevRead(eglGetCurrentSurface(EGL_READ)),
        prevContext(eglGetCurrentContext())
    {
        eglMakeCurrent(display, draw, read, context);
    }

    /**
     * @brief Restores the captured EGL binding.
     */
    ~RGLMakeCurrent() noexcept
    {
        restore();
    }

    /**
     * @brief Restores the EGL binding captured at construction.
     *
     * Has no effect if already restored (either by a prior call or by the destructor).
     *
     * @return `true` if the state was restored by this call, `false` if it had already been restored.
     */
    bool restore() noexcept;

private:
    EGLDisplay prevDisplay;
    EGLSurface prevDraw;
    EGLSurface prevRead;
    EGLContext prevContext;
    bool restored { false };
};

#endif // RGLMAKECURRENT_H
