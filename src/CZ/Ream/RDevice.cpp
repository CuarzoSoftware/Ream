#include <CZ/Ream/RResourceTracker.h>
#include <CZ/Ream/RDevice.h>
#include <CZ/Ream/RCore.h>

using namespace CZ;

RDevice::~RDevice() noexcept
{
    RResourceTrackerSub(RResourceType::RDeviceRes);
}

RGLDevice *RDevice::asGL() noexcept
{
    if (core().graphicsAPI() == RGraphicsAPI::GL)
        return (RGLDevice*)this;
    return nullptr;
}

RDevice::RDevice(RCore &core) noexcept :
    m_core(core)
{
    RResourceTrackerAdd(RResourceType::RDeviceRes);
}
