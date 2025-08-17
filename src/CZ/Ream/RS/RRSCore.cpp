#include <CZ/Ream/DRM/RDRMPlatformHandle.h>
#include <CZ/Ream/RS/RRSDevice.h>
#include <CZ/Ream/RS/RRSCore.h>
#include <CZ/Utils/CZVectorUtils.h>

using namespace CZ;

RRSCore::RRSCore(const Options &options) noexcept : RCore(options)
{
    m_options.graphicsAPI = RGraphicsAPI::RS;
}

RRSCore::~RRSCore() noexcept
{
    unitDevices();
}

bool RRSCore::init() noexcept
{
    return initDevices();
}

bool RRSCore::initDevices() noexcept
{
    if (platform() == RPlatform::Wayland)
    {
        m_mainDevice = RRSDevice::Make(*this, -1, nullptr);

        if (m_mainDevice)
            m_devices.emplace_back(m_mainDevice);
    }
    else // DRM
    {
        for (auto &handle : options().platformHandle->asDRM()->fds())
        {
            auto *dev { RRSDevice::Make(*this, handle.fd, handle.userData) };

            if (dev)
            {
                // Can be any device
                m_mainDevice = dev;
                m_devices.emplace_back(dev);
            }
        }
    }

    return !m_devices.empty();
}

void RRSCore::unitDevices() noexcept
{
    CZVectorUtils::DeleteAndPopBackAll(m_devices);
}
