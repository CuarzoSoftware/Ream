#ifndef CZ_RVKDEVICE_H
#define CZ_RVKDEVICE_H

#include <CZ/Ream/RDevice.h>
#include <vulkan/vulkan.h>

class CZ::RVKDevice final : public RDevice
{
public:
    void wait() noexcept override { /* TODO */ }
    RVKCore &core() noexcept { return (RVKCore&)m_core; }
    VkPhysicalDevice physicalDevice() const noexcept { return m_physicalDevice; }

    bool hasExtension(const char *extension) const noexcept;
private:
    friend class RVKCore;
    static RVKDevice *Make(RVKCore &core, VkPhysicalDevice physicalDevice) noexcept;
    RVKDevice(RVKCore &core, VkPhysicalDevice physicalDevice) noexcept;
    std::shared_ptr<RPainter> makePainter(std::shared_ptr<RSurface> surface) noexcept override;
    bool init() noexcept;
    bool initPropsAndFeatures() noexcept;
    bool initExtensions() noexcept;
    bool initDRM() noexcept;

    VkPhysicalDevice m_physicalDevice;
    VkPhysicalDeviceProperties m_properties {};
    VkPhysicalDeviceFeatures m_features {};
    std::vector<VkExtensionProperties> m_extensions;
    std::vector<const char *> m_requiredExtensions;
    UInt32 m_score {};
};

#endif // CZ_RVKDEVICE_H
