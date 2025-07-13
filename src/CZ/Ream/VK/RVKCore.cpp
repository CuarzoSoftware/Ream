#include <CZ/Ream/RVersion.h>
#include <CZ/Ream/RLog.h>
#include <CZ/Ream/VK/RVKCore.h>
#include <CZ/Ream/VK/RVKDevice.h>
#include <algorithm>
#include <string.h>

using namespace CZ;

RVKCore::RVKCore(const Options &options) noexcept : RCore(options)
{
    m_options.graphicsAPI = RGraphicsAPI::VK;
}

bool RVKCore::init() noexcept
{
    if (initInstanceExtensions() &&
        initInstanceValidationLayers() &&
        initInstance() &&
        initDevices())
        return true;

    RLog(CZFatal, CZLN, "Failed to initialize RVKCore");

    return false;
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

    RLog(CZDebug, "VkInstance Extensions:");

    for (const auto &ext : m_instanceExtensions)
        RLog(CZDebug, " - {}", ext.extensionName);

    for (const auto *reqExt : RequiredInstanceExtensions)
    {
        auto it = std::find_if(
            m_instanceExtensions.begin(),
            m_instanceExtensions.end(),
            [&reqExt](const auto &ext) -> bool{
                return strcmp(reqExt, ext.extensionName) == 0;
            });

        if (it == m_instanceExtensions.end())
        {
            RLog(CZError, CZLN, "Could not find required instance extension: {}", reqExt);
            //std::cerr << "Install vulkan-validation-layers." << std::endl;
            return false;
        }
    }

    return true;
}

bool RVKCore::initInstanceValidationLayers() noexcept
{
    uint32_t count { 0 };

    if (vkEnumerateInstanceLayerProperties(&count, nullptr) != VK_SUCCESS)
    {
        RLog(CZError, CZLN, "Failed to retrieve VkInstance validation layer count");
        return false;
    }

    m_instanceValidationLayers.resize(count);

    if (vkEnumerateInstanceLayerProperties(&count, m_instanceValidationLayers.data()) != VK_SUCCESS)
    {
        RLog(CZError, CZLN, "Failed to retrieve VkInstance validation layers");
        return false;
    }

    RLog(CZDebug, "Vk Instance Validation Layers:");

    for (const auto &ext : m_instanceValidationLayers)
        RLog(CZDebug, " - {}", ext.layerName);

    for (const auto *reqExt : RequiredValidationLayers)
    {
        auto it = std::find_if(
            m_instanceValidationLayers.begin(),
            m_instanceValidationLayers.end(),
            [&reqExt](const auto &ext) -> bool{
                return strcmp(reqExt, ext.layerName) == 0;
            });

        if (it == m_instanceValidationLayers.end())
        {
            RLog(CZError, "Could not find required validation layer: {}", reqExt);
            return false;
        }
    }

    return true;
}

bool RVKCore::initInstance() noexcept
{
    VkApplicationInfo appInfo {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = CZ_REAM_VERSION_SERIAL;
    appInfo.apiVersion = VK_API_VERSION_1_1;

    VkInstanceCreateInfo createInfo {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = RequiredInstanceExtensions.size();
    createInfo.ppEnabledExtensionNames = RequiredInstanceExtensions.data();
    createInfo.enabledLayerCount = RequiredValidationLayers.size();
    createInfo.ppEnabledLayerNames = RequiredValidationLayers.data();

    if (vkCreateInstance(&createInfo, nullptr, &m_instance) != VK_SUCCESS)
    {
        RLog(CZError, CZLN, "Failed to create VkInstance");
        return false;
    }

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
