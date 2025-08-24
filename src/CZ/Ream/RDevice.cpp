#include <CZ/Ream/RResourceTracker.h>
#include <CZ/Ream/RDevice.h>
#include <CZ/Ream/RCore.h>
#include <gbm.h>
#include <xf86drm.h>

using namespace CZ;

RDevice::~RDevice() noexcept
{
    RResourceTrackerSub(RResourceType::RDeviceRes);

    if (gbmDevice())
    {
        gbm_device_destroy(m_gbmDevice);
        m_gbmDevice = nullptr;
    }

    if (core().platform() == RPlatform::Wayland)
    {
        if (drmFd() >= 0)
        {
            close(drmFd());
            m_drmFd = -1;
        }
    }
    // Else: fds are closed by the RDRMPlatformHandle

    if (drmFdReadOnly() >= 0)
    {
        close(drmFdReadOnly());
        m_drmFdReadOnly = -1;
    }
}

RGLDevice *RDevice::asGL() noexcept
{
    if (core().graphicsAPI() == RGraphicsAPI::GL)
        return (RGLDevice*)this;
    return nullptr;
}

RRSDevice *RDevice::asRS() noexcept
{
    if (core().graphicsAPI() == RGraphicsAPI::RS)
        return (RRSDevice*)this;
    return nullptr;
}

RDevice::RDevice(RCore &core) noexcept :
    m_core(core)
{
    RResourceTrackerAdd(RResourceType::RDeviceRes);
}

void RDevice::setDRMDriverName(int fd) noexcept
{
    drmVersion *version { drmGetVersion(fd) };

    if (version)
    {
        if (strcmp(version->name, "i915") == 0)
            m_driver = RDriver::i915;
        else if (strcmp(version->name, "nouveau") == 0)
            m_driver = RDriver::nouveau;
        else if (strcmp(version->name, "lima") == 0)
            m_driver = RDriver::lima;
        else if (strcmp(version->name, "nvidia-drm") == 0)
            m_driver = RDriver::nvidia;
        else if (strcmp(version->name, "nvidia") == 0)
            m_driver = RDriver::nvidia;
        else if (strcmp(version->name, "amdgpu") == 0)
            m_driver = RDriver::amdgpu;
        else if (strcmp(version->name, "radeon") == 0)
            m_driver = RDriver::radeon;

        if (version->name)
            m_drmDriverName = version->name;

        drmFreeVersion(version);
    }
}
