#ifndef RVKCORE_H
#define RVKCORE_H

#include <CZ/Ream/RCore.h>
#include <vulkan/vulkan.h>

static inline constexpr std::array<const char *, 3> RequiredInstanceExtensions
{
    "VK_EXT_debug_utils",
    "VK_KHR_surface",
    "VK_KHR_wayland_surface"
};

static inline constexpr std::array<const char *, 1> RequiredDeviceExtensions
{
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

static inline constexpr std::array<const char *, 1> RequiredValidationLayers
{
    "VK_LAYER_KHRONOS_validation"
};

class CZ::RVKCore final : public RCore
{
public:
    RVKDevice *mainDevice() const noexcept { return (RVKDevice*)m_mainDevice; }
    virtual void clearGarbage() noexcept override {};
private:
    friend class RCore;
    RVKCore(const Options &options) noexcept;
    virtual bool init() noexcept override;
    bool initInstanceExtensions() noexcept;
    bool initInstanceValidationLayers() noexcept;
    bool initInstance() noexcept;
    bool initDevices() noexcept;
    VkInstance m_instance { VK_NULL_HANDLE };
    std::vector<VkExtensionProperties> m_instanceExtensions;
    std::vector<VkLayerProperties> m_instanceValidationLayers;
};

#endif // RVKCORE_H
