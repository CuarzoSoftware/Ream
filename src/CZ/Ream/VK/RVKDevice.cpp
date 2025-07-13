#include <CZ/Ream/VK/RVKDevice.h>
#include <CZ/Ream/VK/RVKCore.h>

using namespace CZ;

RPainter *RVKDevice::painter() const noexcept
{

}

RVKDevice *RVKDevice::Make(RVKCore &core, VkPhysicalDevice physicalDevice) noexcept
{
    auto device { new RVKDevice(core, physicalDevice) };

    if (device->init())
        return device;

    delete device;
    return nullptr;
}

RVKDevice::RVKDevice(RVKCore &core, VkPhysicalDevice physicalDevice) noexcept :
    RDevice(core),
    m_physicalDevice(physicalDevice)
{

}

bool RVKDevice::init() noexcept
{

}
