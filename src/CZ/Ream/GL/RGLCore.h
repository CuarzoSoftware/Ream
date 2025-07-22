#ifndef RGLCORE_H
#define RGLCORE_H

#include <CZ/Ream/RCore.h>
#include <CZ/Ream/EGL/REGLExtensions.h>
#include <CZ/Ream/GL/RGLExtensions.h>

class CZ::RGLCore : public RCore
{
public:
    ~RGLCore();
    RGLDevice *mainDevice() const noexcept { return (RGLDevice*)m_mainDevice; }
    const REGLClientExtensions &clientEGLExtensions() const noexcept { return m_clientEGLExtensions; }
    const REGLClientProcs &clientEGLProcs() const noexcept { return m_clientEGLProcs; }

    void clearGarbage() noexcept override;
    void freeThread() noexcept;
private:
    friend class RCore;
    friend struct RGLThreadDataManager;
    friend class RGLContextDataManager;
    bool init() noexcept override;
    bool initClientEGLExtensions() noexcept;
    bool initDevices() noexcept;

    void unitDevices() noexcept;
    RGLCore(const Options &options) noexcept;
    REGLClientExtensions m_clientEGLExtensions {};
    REGLClientProcs m_clientEGLProcs {};
};

#endif // RGLCORE_H
