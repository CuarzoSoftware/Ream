#include <CZ/Ream/DRM/RDRMPlatformHandle.h>
#include <CZ/Ream/WL/RWLPlatformHandle.h>
#include <CZ/Ream/RS/RRSDevice.h>
#include <CZ/Ream/RS/RRSCore.h>
#include <CZ/Utils/CZVectorUtils.h>
#include <wayland-client-protocol.h>

using namespace CZ;

RRSCore::RRSCore(const Options &options) noexcept : RCore(options)
{
    m_options.graphicsAPI = RGraphicsAPI::RS;
}

RRSCore::~RRSCore() noexcept
{
    unitDevices();

    if (m_shm)
    {
        wl_shm_destroy(m_shm);
        m_shm = nullptr;
    }
}

bool RRSCore::init() noexcept
{
    if (platform() == RPlatform::Wayland)
        initWL();

    return initDevices();
}

static wl_registry_listener listener
{
    .global = [](void *data, wl_registry *registry, UInt32 name, const char *interface, UInt32 version)
    {
        wl_shm **shm { (wl_shm**)data };

        if (!*shm && strcmp(wl_shm_interface.name, interface) == 0)
            *shm = (wl_shm*)wl_registry_bind(registry, name, &wl_shm_interface, version);
    },
    .global_remove = [](auto, auto, auto){}
};

bool RRSCore::initWL() noexcept
{
    auto *dpy { options().platformHandle->asWL()->wlDisplay() };
    auto registry { wl_display_get_registry(dpy) };
    wl_registry_add_listener(registry, &listener, &m_shm);
    wl_display_roundtrip(dpy);
    wl_registry_destroy(registry);
    return true;
}

bool RRSCore::initDevices() noexcept
{
    if (platform() == RPlatform::Wayland)
    {
        m_mainDevice = RRSDevice::Make(*this, -1, nullptr);

        if (m_mainDevice)
            m_devices.emplace_back(m_mainDevice);
    }
    else if (platform() == RPlatform::DRM)
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
    else // Offscreen
    {
        m_mainDevice = RRSDevice::Make(*this, -1, nullptr);
        m_devices.emplace_back(m_mainDevice);
    }

    return !m_devices.empty();
}

void RRSCore::unitDevices() noexcept
{
    CZVectorUtils::DeleteAndPopBackAll(m_devices);
}
