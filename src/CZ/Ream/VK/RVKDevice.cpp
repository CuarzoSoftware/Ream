#include "Utils/CZStringUtils.h"
#include <CZ/Ream/DRM/RDRMPlatformHandle.h>
#include <CZ/Ream/VK/RVKDevice.h>
#include <CZ/Ream/VK/RVKCore.h>
#include <fcntl.h>
#include <sys/sysmacros.h>
#include <sys/stat.h>
#include <xf86drm.h>

using namespace CZ;

bool RVKDevice::hasExtension(const char *extension) const noexcept
{
    for (const auto &ext : m_extensions)
        if (strcmp(ext.extensionName, extension) == 0)
            return true;
    return false;
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

std::shared_ptr<RPainter> RVKDevice::makePainter(std::shared_ptr<RSurface>) noexcept
{
    // TODO
    return {};
}

bool RVKDevice::init() noexcept
{
    return initPropsAndFeatures() &&
           initExtensions() &&
           initDRM();
}

bool RVKDevice::initPropsAndFeatures() noexcept
{
    vkGetPhysicalDeviceProperties(physicalDevice(), &m_properties);
    vkGetPhysicalDeviceFeatures(physicalDevice(), &m_features);

    if (m_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU)
    {
        RLog(CZWarning, CZLN, "{}: Unsupported Vulkan device type (CPU). Ignoring it...", m_properties.deviceName);
        return false;
    }

    if (!m_features.geometryShader)
        return false;

    return true;
}

bool RVKDevice::initExtensions() noexcept
{
    UInt32 count { 0 };

    if (vkEnumerateDeviceExtensionProperties(physicalDevice(), nullptr, &count, nullptr) != VK_SUCCESS)
    {
        RLog(CZError, CZLN, "{}: Failed to retreive device extension count", m_properties.deviceName);
        return false;
    }

    m_extensions.resize(count);

    if (vkEnumerateDeviceExtensionProperties(physicalDevice(), nullptr, &count, m_extensions.data()) != VK_SUCCESS)
    {
        RLog(CZError, CZLN, "{}: Failed to retreive device extensions", m_properties.deviceName);
        return false;
    }

    std::vector<const char*> optionalExtensions;

    if (m_core.platform() == RPlatform::Wayland)
    {
        optionalExtensions = { VK_EXT_PHYSICAL_DEVICE_DRM_EXTENSION_NAME };
        m_requiredExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    }
    else
    {
        optionalExtensions = {
            VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME,
            VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME
            VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
            VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME
            VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME,
            VK_EXT_QUEUE_FAMILY_FOREIGN_EXTENSION_NAME,
            VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME,
        };
        m_requiredExtensions = { VK_EXT_PHYSICAL_DEVICE_DRM_EXTENSION_NAME };
    }

    for (const auto *extension : m_requiredExtensions)
    {
        if (!hasExtension(extension))
        {
            RLog(CZError, CZLN, "{}: Missing required device extension {}", m_properties.deviceName, extension);
            return false;
        }
    }

    for (const auto *extension : optionalExtensions)
    {
        if (hasExtension(extension))
            m_requiredExtensions.emplace_back(extension);
        else
            RLog(CZWarning, CZLN, "{}: Optional device extension not available {}", m_properties.deviceName, extension);
    }

    return true;
}

bool RVKDevice::initDRM() noexcept
{
    if (!hasExtension(VK_EXT_PHYSICAL_DEVICE_DRM_EXTENSION_NAME))
        goto end;

    {
        VkPhysicalDeviceProperties2 props2 {};
        props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;

        VkPhysicalDeviceDrmPropertiesEXT drmProps {};
        drmProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRM_PROPERTIES_EXT;

        drmProps.pNext = props2.pNext;
        props2.pNext = &drmProps;

        vkGetPhysicalDeviceProperties2(m_physicalDevice, &props2);

        if (m_core.platform() == RPlatform::Wayland)
        {
            // TODO: Open renderNode, otherwise primary
        }
        else
        {
            const auto primaryDev { makedev(drmProps.primaryMajor, drmProps.primaryMinor) };
            const auto renderDev { makedev(drmProps.renderMajor, drmProps.renderMinor) };

            for (auto handle : m_core.options().platformHandle->asDRM()->fds())
            {
                struct stat devStat { };

                if (fstat(handle.fd, &devStat) != 0)
                    continue;

                if (devStat.st_rdev == primaryDev || devStat.st_rdev == renderDev)
                {
                    m_id = devStat.st_rdev;
                    m_drmFd = handle.fd;
                    m_drmUserData = handle.userData;

                    drmDevicePtr device;

                    if (drmGetDevice(m_drmFd, &device) == 0)
                    {
                        if (device->available_nodes & (1 << DRM_NODE_PRIMARY) && device->nodes[DRM_NODE_PRIMARY])
                            m_drmNode = device->nodes[DRM_NODE_PRIMARY];
                        else if (device->available_nodes & (1 << DRM_NODE_RENDER) && device->nodes[DRM_NODE_RENDER])
                            m_drmNode = device->nodes[DRM_NODE_RENDER];

                        drmFreeDevice(&device);
                    }

                    if (m_drmNode.empty())
                        m_drmNode = "Unknown Device";
                    else
                    {
                        m_drmFdReadOnly = open(m_drmNode.c_str(), O_RDONLY | O_CLOEXEC);
                        log = RLog.newWithContext(CZStringUtils::SubStrAfterLastOf(m_drmNode, "/"));
                    }

                    setDRMDriverName(m_drmFd);
                    return true;
                }
            }

            RLog(CZError, CZLN, "{}: Failed to associate device with a DRM node. Ignoring it...", m_properties.deviceName);
        }
    }

end:
    if (m_core.platform() == RPlatform::Wayland)
        return true;

    // Mandatory for the DRM platform
    return false;
}
