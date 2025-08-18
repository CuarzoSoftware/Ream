#include <CZ/Ream/RVersion.h>
#include <CZ/Ream/RLog.h>
#include <CZ/Ream/VK/RVKCore.h>
#include <CZ/Ream/VK/RVKDevice.h>
#include <CZ/Ream/VK/RVKExtensions.h>
#include <CZ/Utils/CZVectorUtils.h>

using namespace CZ;

RVKCore::~RVKCore() noexcept
{
    if (m_debugMsg != VK_NULL_HANDLE && m_instanceProcs.destroyDebugUtilsMessengerEXT)
    {
        m_instanceProcs.destroyDebugUtilsMessengerEXT(m_instance, m_debugMsg, nullptr);
        m_debugMsg = VK_NULL_HANDLE;
        RLog(CZTrace, CZLN, "VkDebugUtilsMessengerEXT destroyed");
    }

    if (m_instance != VK_NULL_HANDLE)
    {
        vkDestroyInstance(m_instance, nullptr);
        m_instance = VK_NULL_HANDLE;
        RLog(CZTrace, CZLN, "VkInstance destroyed");
    }
}

bool RVKCore::hasInstanceExtension(std::string_view extension) const noexcept
{
    for (const auto &ext : m_instanceExtensions)
        if (ext.extensionName == extension)
            return true;

    return false;
}

bool RVKCore::hasValidationLayer(std::string_view layer) const noexcept
{
    for (const auto &ext : m_instanceValidationLayers)
        if (ext.layerName == layer)
            return true;

    return false;
}

RVKCore::RVKCore(const Options &options) noexcept : RCore(options)
{
    m_options.graphicsAPI = RGraphicsAPI::VK;
}

bool RVKCore::init() noexcept
{
    vkLog = vkLog.newWithContext("VK");

    if (initInstanceExtensions() &&
        initInstanceValidationLayers() &&
        initInstance() &&
        initInstanceProcs() &&
        initDebugger() &&
        initDevices())
        return true;

    RLog(CZFatal, CZLN, "Failed to initialize RVKCore");

    return false;
}

bool RVKCore::initInstanceProcs() noexcept
{
    m_instanceProcs.createDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT");
    m_instanceProcs.destroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT");
    return true;
}

bool RVKCore::initInstanceExtensions() noexcept
{
    UInt32 count { 0 };

    if (vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr) != VK_SUCCESS)
    {
        RLog(CZError, CZLN, "Failed to retrieve VkInstance extension count");
        return false;
    }

    m_instanceExtensions.resize(count);

    if (vkEnumerateInstanceExtensionProperties(nullptr, &count, m_instanceExtensions.data()) != VK_SUCCESS)
    {
        RLog(CZError, CZLN, "Failed to retrieve VkInstance extensions");
        return false;
    }

    if (vkLog.level() >= CZInfo)
    {
        vkLog(CZInfo, "VkInstance Extensions:");

        for (const auto &ext : m_instanceExtensions)
            vkLog(CZInfo, " - {}", ext.extensionName);
    }

    if (platform() == RPlatform::Wayland)
    {
        m_requiredInstanceExtensions = {
            VK_KHR_SURFACE_EXTENSION_NAME,
            "VK_KHR_wayland_surface"
        };
    }
    else
    {
        // TODO: Add required DRM extensions
        m_requiredInstanceExtensions = {
            VK_KHR_SURFACE_EXTENSION_NAME,
        };
    }

    for (const auto *ext : m_requiredInstanceExtensions)
    {
        if (!hasInstanceExtension(ext))
        {
            RLog(CZError, CZLN, "Missing required VkInstance extension: {}", ext);
            return false;
        }
    }

    if (vkLog.level() > CZSilent && hasInstanceExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME))
        m_requiredInstanceExtensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    return true;
}

bool RVKCore::initInstanceValidationLayers() noexcept
{
    if (vkLog.level() <= CZSilent)
        return true;

    uint32_t count { 0 };

    if (vkEnumerateInstanceLayerProperties(&count, nullptr) != VK_SUCCESS)
    {
        RLog(CZWarning, CZLN, "Failed to retrieve VkInstance validation layer count");
        return true;
    }

    m_instanceValidationLayers.resize(count);

    if (vkEnumerateInstanceLayerProperties(&count, m_instanceValidationLayers.data()) != VK_SUCCESS)
    {
        RLog(CZWarning, CZLN, "Failed to retrieve VkInstance validation layers");
        return true;
    }

    if (vkLog.level() >= CZInfo)
    {
        vkLog(CZInfo, "Vk Instance Validation Layers:");

        for (const auto &ext : m_instanceValidationLayers)
            vkLog(CZInfo, " - {}", ext.layerName);
    }

    m_requiredValidationLayers.emplace_back("VK_LAYER_KHRONOS_validation");

    for (size_t i = 0; i < m_requiredValidationLayers.size();)
    {
        if (!hasValidationLayer(m_requiredValidationLayers[i]))
        {
            RLog(CZWarning, CZLN, "Missing required Vulkan validation layer: {}. Ignoring it...", m_requiredValidationLayers[i]);

            if (strcmp(m_requiredValidationLayers[i], "VK_LAYER_KHRONOS_validation") == 0)
                RLog(CZWarning, CZLN, "Install VK_LAYER_KHRONOS_validation using your package manager");

            m_requiredValidationLayers[i] = m_requiredValidationLayers.back();
            m_requiredValidationLayers.pop_back();
        }
        else
            i++;
    }

    return true;
}

bool RVKCore::initDebugger() noexcept
{
    if (vkLog.level() <= CZSilent || !m_instanceProcs.createDebugUtilsMessengerEXT || !hasInstanceExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME))
        return true;

    VkDebugUtilsMessengerCreateInfoEXT info {};
    info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    if (vkLog.level() >= CZWarning)
    {
        info.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
        if (vkLog.level() >= CZInfo)
        {
            info.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
            if (vkLog.level() >= CZTrace)
                info.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
        }
    }
    info.pUserData = this;
    info.pfnUserCallback = [](VkDebugUtilsMessageSeverityFlagBitsEXT           messageSeverity,
                              VkDebugUtilsMessageTypeFlagsEXT                  messageTypes,
                              const VkDebugUtilsMessengerCallbackDataEXT*      pCallbackData,
                              void*                                            pUserData) -> VkBool32
    {
        CZ_UNUSED(messageTypes)
        auto *core { static_cast<RVKCore*>(pUserData) };
        CZLogLevel logLevel;

        switch (messageSeverity)
        {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
            logLevel = CZTrace;
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
            logLevel = CZInfo;
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            logLevel = CZWarning;
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            logLevel = CZError;
            break;
        default:
            logLevel = CZDebug;
            break;
        }

        core->vkLog(logLevel, "{} : {}", pCallbackData->pMessage, pCallbackData->pMessageIdName);

        for (UInt32 i = 0; i < pCallbackData->objectCount; i++)
            if (pCallbackData->pObjects[i].pObjectName)
                core->vkLog(logLevel, "    Obj: {}", pCallbackData->pObjects[i].pObjectName);

        return false;
    };

    if (m_instanceProcs.createDebugUtilsMessengerEXT(m_instance, &info, nullptr, &m_debugMsg) != VK_SUCCESS)
        RLog(CZWarning, CZLN, "Failed to create VkDebugUtilsMessengerEXT");
    else
        RLog(CZTrace, CZLN, "VkDebugUtilsMessengerEXT created");

    return true;
}

bool RVKCore::initInstance() noexcept
{
    if (vkLog.level() <= CZSilent || m_requiredValidationLayers.empty() || !hasInstanceExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME))
        m_requiredInstanceExtensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    VkApplicationInfo appInfo {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = CZ_REAM_VERSION_SERIAL;
    appInfo.apiVersion = VK_API_VERSION_1_1;

    VkInstanceCreateInfo createInfo {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = m_requiredInstanceExtensions.size();
    createInfo.ppEnabledExtensionNames = m_requiredInstanceExtensions.data();
    createInfo.enabledLayerCount = m_requiredValidationLayers.size();
    createInfo.ppEnabledLayerNames = m_requiredValidationLayers.data();

    if (vkCreateInstance(&createInfo, nullptr, &m_instance) != VK_SUCCESS)
    {
        RLog(CZError, CZLN, "Failed to create VkInstance");
        return false;
    }
    RLog(CZTrace, CZLN, "VkInstance created");

    return true;
}

bool RVKCore::initDevices() noexcept
{
    UInt32 count { 0 };

    if (vkEnumeratePhysicalDevices(m_instance, &count, nullptr) != VK_SUCCESS)
    {
        RLog(CZError, CZLN, "Failed to retrieve the physical device count");
        return false;
    }

    std::vector<VkPhysicalDevice> physicalDevices { count };

    if (vkEnumeratePhysicalDevices(m_instance, &count, physicalDevices.data()) != VK_SUCCESS)
    {
        RLog(CZError, CZLN, "Failed to retrieve physical devices");
        return false;
    }

    m_devices.reserve(count);
    UInt32 maxScore { 0 };

    for (auto &physicalDevice : physicalDevices)
    {
        auto *device { RVKDevice::Make(*this, physicalDevice) };

        if (device)
        {
            if (device->m_score > maxScore)
            {
                maxScore = device->m_score;
                m_mainDevice = device;
            }

            m_devices.emplace_back(device);
        }
    }

    if (m_devices.empty())
    {
        RLog(CZError, CZLN, "Failed to initialize at least one device");
        return false;
    }

    return true;
}
