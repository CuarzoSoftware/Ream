#ifndef RVKCORE_H
#define RVKCORE_H

#include <CZ/Ream/VK/RVKExtensions.h>
#include <CZ/Core/CZLogger.h>
#include <CZ/Ream/RCore.h>
#include <vulkan/vulkan.h>

/**
 * @brief Vulkan implementation of RCore, obtained via RCore::asVK().
 *
 * Owns the VkInstance (plus optional debug messenger and validation layers), enumerates the
 * physical devices, and wraps each as an RVKDevice, selecting the highest-scoring one as the
 * main device.
 */
class CZ::RVKCore final : public RCore
{
public:
    ~RVKCore() noexcept;

    /**
     * @brief Returns the main RVKDevice (RCore::mainDevice() downcast to the Vulkan backend).
     */
    RVKDevice *mainDevice() const noexcept { return (RVKDevice*)m_mainDevice; }

    /**
     * @brief Drains each device's fence-tracked deferred-destruction queue.
     *
     * Forwards to RVKDevice::clearGarbage() for every device.
     */
    virtual void clearGarbage() noexcept override;

    /**
     * @brief Returns true if @p extension is among the available VkInstance extensions.
     */
    bool hasInstanceExtension(std::string_view extension) const noexcept;

    /**
     * @brief Returns true if @p layer is among the available VkInstance validation layers.
     */
    bool hasValidationLayer(std::string_view layer) const noexcept;

    /**
     * @brief Returns the underlying VkInstance handle.
     */
    VkInstance instance() const noexcept { return m_instance; }

    // Enabled instance extensions/layers (kept alive for skgpu::VulkanExtensions::init).
    const std::vector<const char *> &enabledInstanceExtensions() const noexcept { return m_requiredInstanceExtensions; }

    /**
     * @brief Logger for the Vulkan backend (verbosity controlled by CZ_REAM_VK_LOG_LEVEL).
     */
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
