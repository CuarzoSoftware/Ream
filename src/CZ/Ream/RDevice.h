#ifndef RDEVICE_H
#define RDEVICE_H

#include <CZ/Ream/RObject.h>
#include <CZ/CZWeak.h>
#include <string>

namespace CZ
{
    class SRMDevice;
}

struct gbm_device;

class CZ::RDevice : public RObject
{
public:

    // Could be -1
    int drmFd() const noexcept { return m_drmFd; }

    // e.g. /dev/dri/card0 or empty
    const std::string &drmNode() const noexcept { return m_drmNode; }

    // Could be nullptr
    gbm_device *gbmDevice() const noexcept { return m_gbmDevice; }

    // Could be nullptr
    SRMDevice *srmDevice() const noexcept { return m_srmDevice; }

    RCore &core() const noexcept { return m_core; }
    RGLDevice *asGL() noexcept;
protected:
    RDevice(RCore &core) noexcept :
        m_core(core) {};
    RCore &m_core;
    int m_drmFd { -1 };
    std::string m_drmNode;
    gbm_device *m_gbmDevice { nullptr };
    CZWeak<SRMDevice> m_srmDevice;
};

#endif // RDEVICE_H
