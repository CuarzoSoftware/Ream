#ifndef RGLDEVICE_H
#define RGLDEVICE_H

#include <CZ/skia/gpu/ganesh/GrDirectContext.h>
#include <CZ/Ream/EGL/REGLExtensions.h>
#include <CZ/Ream/GL/RGLExtensions.h>
#include <CZ/Ream/GL/RGLContext.h>
#include <CZ/Ream/RDevice.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

class CZ::RGLDevice : public RDevice
{
public:
    RGLCore &core() noexcept { return (RGLCore&)m_core; }
    EGLDisplay eglDisplay() const noexcept { return m_eglDisplay; }
    EGLContext eglContext() const noexcept;
    EGLDeviceEXT eglDevice() const noexcept { return m_eglDevice; }
    sk_sp<GrDirectContext> skContext() const noexcept;
    const REGLDisplayProcs &eglDisplayProcs() const noexcept { return m_eglDisplayProcs; }
    const REGLDisplayExtensions &eglDisplayExtensions() const noexcept { return m_eglDisplayExtensions; }
    const REGLDeviceExtensions &eglDeviceExtensions() const noexcept { return m_eglDeviceExtensions; }
    const RGLExtensions &glExtensions() const noexcept { return m_glExtensions; }
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
