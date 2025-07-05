#ifndef RGLDEVICE_H
#define RGLDEVICE_H

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
    const REGLDisplayProcs &eglDisplayProcs() const noexcept { return m_eglDisplayProcs; }
    const REGLDisplayExtensions &eglDisplayExtensions() const noexcept { return m_eglDisplayExtensions; }
    const REGLDeviceExtensions &eglDeviceExtensions() const noexcept { return m_eglDeviceExtensions; }
    const RGLExtensions &glExtensions() const noexcept { return m_glExtensions; }
private:
    friend class RGLCore;
    friend struct RGLThreadDataManager;
    static RGLDevice *Make(RGLCore &core, int drmFd, void *userData) noexcept;
    RGLDevice(RGLCore &core, int drmFd, void *userData) noexcept;
    ~RGLDevice();
    bool init() noexcept;

    bool initWL() noexcept;
    bool initEGLDisplayWL() noexcept;

    bool initDRM() noexcept;
    bool initEGLDisplayDRM() noexcept;

    bool initEGLDisplayExtensions() noexcept;
    bool initEGLContext() noexcept;
    bool initGLExtensions() noexcept;
    bool initEGLDisplayProcs() noexcept;
    bool initPainter() noexcept;

    RPainter *painter() const noexcept override;

    class ThreadData : public RGLContextData
    {
    public:
        ThreadData(RGLDevice *device) noexcept;
        std::shared_ptr<RGLPainter> painter;
    };

    mutable std::shared_ptr<RGLContextDataManager> m_threadData;
    EGLDisplay m_eglDisplay { EGL_NO_DISPLAY };
    EGLDeviceEXT m_eglDevice { EGL_NO_DEVICE_EXT };
    EGLContext m_eglContext { EGL_NO_CONTEXT };
    REGLDisplayProcs m_eglDisplayProcs {};
    REGLDisplayExtensions m_eglDisplayExtensions {};
    REGLDeviceExtensions m_eglDeviceExtensions {};
    RGLExtensions m_glExtensions {};
    void *m_drmUserData {};
};

#endif // RGLDEVICE_H
