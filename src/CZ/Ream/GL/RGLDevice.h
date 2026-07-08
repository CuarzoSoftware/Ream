#ifndef RGLDEVICE_H
#define RGLDEVICE_H

#include <CZ/skia/gpu/ganesh/GrDirectContext.h>
#include <CZ/Ream/EGL/REGLExtensions.h>
#include <CZ/Ream/GL/RGLExtensions.h>
#include <CZ/Ream/GL/RGLContext.h>
#include <CZ/Ream/RDevice.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

/**
 * @brief OpenGL backend implementation of RDevice.
 *
 * Represents a single GPU driven through EGL and OpenGL ES. It owns the device's EGL display,
 * the main-thread EGL context and Skia GrDirectContext, and the associated EGL/GL extension
 * tables. Because contexts are per-thread, eglContext() and skContext() return the objects
 * bound to the calling thread (lazily created as shared contexts on non-main threads).
 *
 * Obtain it from an RDevice via RDevice::asGL().
 */
class CZ::RGLDevice : public RDevice
{
public:
    /**
     * @brief Returns the owning core as an RGLCore.
     */
    RGLCore &core() noexcept { return (RGLCore&)m_core; }

    /**
     * @brief Returns the EGL display associated with this device.
     */
    EGLDisplay eglDisplay() const noexcept { return m_eglDisplay; }

    /**
     * @brief Returns the EGL context for the calling thread.
     *
     * On the main thread this is the device's primary context; on other threads a shared context
     * is created on first use. Returns EGL_NO_CONTEXT if none could be obtained.
     */
    EGLContext eglContext() const noexcept;

    /**
     * @brief Returns the EGLDeviceEXT backing this device, if any.
     */
    EGLDeviceEXT eglDevice() const noexcept { return m_eglDevice; }

    /**
     * @brief Returns the Skia GrDirectContext for the calling thread.
     *
     * Like eglContext(), this is per-thread: the main-thread context on the main thread, or a
     * thread-local context created alongside the shared EGL context otherwise.
     */
    sk_sp<GrDirectContext> skContext() const noexcept;

    /**
     * @brief Returns the display-level EGL function pointers for this device.
     */
    const REGLDisplayProcs &eglDisplayProcs() const noexcept { return m_eglDisplayProcs; }

    /**
     * @brief Returns the display-level EGL extensions supported by this device.
     */
    const REGLDisplayExtensions &eglDisplayExtensions() const noexcept { return m_eglDisplayExtensions; }

    /**
     * @brief Returns the device-level EGL extensions supported by this device.
     */
    const REGLDeviceExtensions &eglDeviceExtensions() const noexcept { return m_eglDeviceExtensions; }

    /**
     * @brief Returns the OpenGL ES extensions supported by this device's context.
     */
    const RGLExtensions &glExtensions() const noexcept { return m_glExtensions; }

    /**
     * @brief Blocks until all GL commands submitted on this device's context have completed.
     *
     * Makes the device's context current and issues a `glFinish()`.
     */
    void wait() noexcept override;
private:
    friend class RGLCore;
    friend struct RGLThreadDataManager;
    friend class RGLProgram;
    friend class RGLShader;
    static RGLDevice *Make(RGLCore &core, int drmFd, void *userData) noexcept;
    RGLDevice(RGLCore &core, int drmFd, void *userData) noexcept;
    ~RGLDevice() noexcept;
    bool init() noexcept;

    bool initWL() noexcept;
    bool initEGLDisplayWL() noexcept;

    bool initDRM() noexcept;
    bool initEGLDisplayDRM() noexcept;

    bool initEGLDisplayExtensions() noexcept;
    bool initEGLContext() noexcept;
    bool initGLExtensions() noexcept;
    bool initEGLDisplayProcs() noexcept;
    bool initDMAFormats() noexcept;
    bool initFormats() noexcept;
    bool initPainter() noexcept;

    std::shared_ptr<RPainter> makePainter(std::shared_ptr<RSurface> surface) noexcept override;

    class ThreadData : public RGLContextData
    {
    public:
        ThreadData(RGLDevice *device) noexcept;
        // Features => Shader/Program
        std::unordered_map<UInt32, std::shared_ptr<RGLShader>> vertShaders;
        std::unordered_map<UInt32, std::shared_ptr<RGLShader>> fragShaders;
        std::unordered_map<UInt32, std::shared_ptr<RGLProgram>> programs;
    };

    mutable std::shared_ptr<RGLContextDataManager> m_threadData;
    EGLDisplay m_eglDisplay { EGL_NO_DISPLAY };
    EGLDeviceEXT m_eglDevice { EGL_NO_DEVICE_EXT };
    EGLContext m_eglContext { EGL_NO_CONTEXT };
    sk_sp<GrDirectContext> m_skContext;
    REGLDisplayProcs m_eglDisplayProcs {};
    REGLDisplayExtensions m_eglDisplayExtensions {};
    REGLDeviceExtensions m_eglDeviceExtensions {};
    RGLExtensions m_glExtensions {};
};

#endif // RGLDEVICE_H
