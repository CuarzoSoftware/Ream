#ifndef RVKCORE_H
#define RVKCORE_H

#include <CZ/Ream/VK/RVKExtensions.h>
#include <CZ/CZLogger.h>
#include <CZ/Ream/RCore.h>
#include <vulkan/vulkan.h>

class CZ::RVKCore final : public RCore
{
public:
    ~RVKCore() noexcept;
    RVKDevice *mainDevice() const noexcept { return (RVKDevice*)m_mainDevice; }
    virtual void clearGarbage() noexcept override {};
    bool hasInstanceExtension(std::string_view extension) const noexcept;
    bool hasValidationLayer(std::string_view layer) const noexcept;
    CZLogger vkLog { "Ream", "CZ_REAM_VK_LOG_LEVEL" };
private:
    friend class RCore;
    RVKCore(const Options &options) noexcept;
    virtual bool init() noexcept override;
    bool initInstanceExtensions() noexcept;
    bool initInstanceValidationLayers() noexcept;
    bool initInstance() noexcept;
    bool initInstanceProcs() noexcept;
    bool initDebugger() noexcept;
    bool initDevices() noexcept;
    VkInstance m_instance { VK_NULL_HANDLE };
    VkDebugUtilsMessengerEXT m_debugMsg { VK_NULL_HANDLE };

    // Available
    std::vector<VkExtensionProperties> m_instanceExtensions;
    std::vector<VkLayerProperties> m_instanceValidationLayers;

    // Required
    std::vector<const char *> m_requiredInstanceExtensions;
    std::vector<const char *> m_requiredValidationLayers;

    RVKInstanceProcs m_instanceProcs {};
};

#endif // RVKCORE_H
