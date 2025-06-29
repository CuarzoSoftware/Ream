#ifndef RGLCORE_H
#define RGLCORE_H

#include <CZ/Ream/RCore.h>
#include <CZ/Ream/GL/RGLExtensions.h>

class CZ::RGLCore : public RCore
{
public:
    ~RGLCore();
    void unbindCurrentThread() noexcept override;
    const std::vector<RGLDevice*> &devices() const noexcept { return (const std::vector<RGLDevice*>&)m_devices; }
    RGLDevice *mainDevice() const noexcept { return (RGLDevice*)m_mainDevice; }
    const REGLClientExtensions &clientEGLExtensions() const noexcept { return m_clientEGLExtensions; }
    const REGLClientProcs &clientEGLProcs() const noexcept { return m_clientEGLProcs; }
private:
    friend class RCore;
    bool init() noexcept override;
    bool initClientEGLExtensions() noexcept;
    bool initDevices() noexcept;

    void unitDevices() noexcept;
    RGLCore(const Options &options) noexcept;
    REGLClientExtensions m_clientEGLExtensions {};
    REGLClientProcs m_clientEGLProcs {};
};

#endif // RGLCORE_H
