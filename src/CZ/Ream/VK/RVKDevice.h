#ifndef CZ_RVKDEVICE_H
#define CZ_RVKDEVICE_H

#include <CZ/Ream/RDevice.h>
#include <vulkan/vulkan.h>

class CZ::RVKDevice final : public RDevice
{
public:
    RVKCore &core() noexcept { return (RVKCore&)m_core; }
    VkPhysicalDevice physicalDevice() const noexcept { return m_physicalDevice; }
private:
    friend class RVKCore;
    RPainter *painter() const noexcept override;
    static RVKDevice *Make(RVKCore &core, VkPhysicalDevice physicalDevice) noexcept;
    RVKDevice(RVKCore &core, VkPhysicalDevice physicalDevice) noexcept;
    bool init() noexcept;

    VkPhysicalDevice m_physicalDevice;
    UInt32 m_score {};
};

#endif // CZ_RVKDEVICE_H
