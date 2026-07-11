#include <CZ/Core/Utils/CZStringUtils.h>
#include <CZ/Ream/DRM/RDRMPlatformHandle.h>
#include <CZ/Ream/SK/RSKContext.h>
#include <CZ/Ream/VK/RVKDevice.h>
#include <CZ/Ream/VK/RVKCore.h>
#include <CZ/Ream/VK/RVKFormat.h>
#include <CZ/Ream/VK/RVKPipeline.h>
#include <CZ/Ream/VK/RVKPainter.h>

#include <CZ/skia/gpu/ganesh/vk/GrVkDirectContext.h>
#include <CZ/skia/gpu/vk/VulkanBackendContext.h>

#include <fcntl.h>
#include <optional>
#include <gbm.h>
#include <sys/sysmacros.h>
#include <sys/stat.h>
#include <xf86drm.h>
#include <drm_fourcc.h>

using namespace CZ;

// -----------------------------------------------------------------------------
// Skia helper that is compiled into libcz-skia but whose header is not exposed.
// skgpu::VulkanMemoryAllocators::Make() builds an AMD-VMA-backed allocator from a
// VulkanBackendContext. The skgpu::ThreadSafe enum is also internal; it is a plain
// `enum class : bool` so this local declaration is ABI-compatible for the call.
// -----------------------------------------------------------------------------
namespace skgpu
{
    enum class ThreadSafe : bool { kNo = false, kYes = true };
    namespace VulkanMemoryAllocators
    {
        sk_sp<VulkanMemoryAllocator> Make(const VulkanBackendContext &, ThreadSafe, std::optional<size_t>);
    }
}

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
    m_mainThread = std::this_thread::get_id();
}

RVKDevice::~RVKDevice() noexcept
{
    if (m_device != VK_NULL_HANDLE)
    {
        vkDeviceWaitIdle(m_device);

        // All fences are signalled after idle: run every pending deferred cleanup, then destroy.
        {
            // Device teardown: render threads are stopped, so draining every thread's entries from
            // here is safe (no concurrent pool access).
            std::lock_guard<std::mutex> lock { m_garbageMutex };
            for (auto &g : m_garbage)
            {
                g.cleanup();
                vkDestroyFence(m_device, g.fence, nullptr);
            }
            m_garbage.clear();
        }

        {
            std::lock_guard<std::mutex> lock { m_skContextsMutex };
            m_skContexts.clear();
        }
        m_pipelines.reset();
        m_skContext.reset();
        m_allocator.reset();

        // Destroy any pending-wait semaphores never consumed by a submission.
        for (auto &pw : m_pendingWaits)
            if (pw.second)
                vkDestroySemaphore(m_device, pw.first, nullptr);
        m_pendingWaits.clear();

        if (m_commandPool != VK_NULL_HANDLE)
            vkDestroyCommandPool(m_device, m_commandPool, nullptr);

        vkDestroyDevice(m_device, nullptr);
        m_device = VK_NULL_HANDLE;
    }
}

std::shared_ptr<RPainter> RVKDevice::makePainter(std::shared_ptr<RSurface> surface) noexcept
{
    if (!surface || !pipelines())
        return {};
    return std::shared_ptr<RPainter>(new RVKPainter(surface, this));
}

RVKPipeline *RVKDevice::pipelines() noexcept
{
    std::lock_guard<std::mutex> lock { m_pipelinesMutex };
    if (!m_pipelines)
        m_pipelines = RVKPipeline::Make(this);
    return m_pipelines.get();
}

void RVKDevice::wait() noexcept
{
    if (m_device == VK_NULL_HANDLE)
        return;

    std::lock_guard<std::mutex> lock { m_queueMutex };
    vkDeviceWaitIdle(m_device);
}

void RVKDevice::clearGarbage() noexcept
{
    if (m_device == VK_NULL_HANDLE)
        return;

    const auto self { std::this_thread::get_id() };
    std::lock_guard<std::mutex> lock { m_garbageMutex };
    for (size_t i = 0; i < m_garbage.size();)
    {
        auto &g { m_garbage[i] };
        // Only free resources on the thread that created them (command-pool external-sync rule).
        if (g.owner == self && vkGetFenceStatus(m_device, g.fence) == VK_SUCCESS)
        {
            g.cleanup();
            vkDestroyFence(m_device, g.fence, nullptr);
            m_garbage[i] = std::move(m_garbage.back());
            m_garbage.pop_back();
        }
        else
            i++;
    }
}

UInt32 RVKDevice::findMemoryType(UInt32 typeBits, VkMemoryPropertyFlags props) const noexcept
{
    for (UInt32 i = 0; i < m_memoryProperties.memoryTypeCount; i++)
    {
        if ((typeBits & (1u << i)) &&
            (m_memoryProperties.memoryTypes[i].propertyFlags & props) == props)
            return i;
    }
    return UINT32_MAX;
}

bool RVKDevice::immediateSubmit(const std::function<void(VkCommandBuffer)> &record) const noexcept
{
    std::lock_guard<std::mutex> lock { m_queueMutex };

    VkCommandBufferAllocateInfo allocInfo {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = m_commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer cmd { VK_NULL_HANDLE };
    if (vkAllocateCommandBuffers(m_device, &allocInfo, &cmd) != VK_SUCCESS)
        return false;

    VkCommandBufferBeginInfo beginInfo {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    bool ok { false };
    VkFence fence { VK_NULL_HANDLE };

    // Consume any semaphores queued via queueWait() so this submission waits on them.
    std::vector<VkSemaphore> waitSems;
    std::vector<VkPipelineStageFlags> waitStages;
    std::vector<VkSemaphore> ownedWaits;
    for (auto &pw : m_pendingWaits)
    {
        waitSems.push_back(pw.first);
        waitStages.push_back(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
        if (pw.second)
            ownedWaits.push_back(pw.first);
    }
    m_pendingWaits.clear();

    if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS)
        goto cleanup;

    record(cmd);

    if (vkEndCommandBuffer(cmd) != VK_SUCCESS)
        goto cleanup;

    {
        VkFenceCreateInfo fenceInfo {};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        if (vkCreateFence(m_device, &fenceInfo, nullptr, &fence) != VK_SUCCESS)
            goto cleanup;

        VkSubmitInfo submit {};
        submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.waitSemaphoreCount = waitSems.size();
        submit.pWaitSemaphores = waitSems.data();
        submit.pWaitDstStageMask = waitStages.data();
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &cmd;

        if (vkQueueSubmit(m_graphicsQueue, 1, &submit, fence) != VK_SUCCESS)
            goto cleanup;

        vkWaitForFences(m_device, 1, &fence, VK_TRUE, UINT64_MAX);
        ok = true;
    }

cleanup:
    if (fence != VK_NULL_HANDLE)
        vkDestroyFence(m_device, fence, nullptr);
    vkFreeCommandBuffers(m_device, m_commandPool, 1, &cmd);
    for (VkSemaphore s : ownedWaits)
        vkDestroySemaphore(m_device, s, nullptr);
    return ok;
}

bool RVKDevice::submitCommand(VkCommandBuffer cmd) const noexcept
{
    std::lock_guard<std::mutex> lock { m_queueMutex };

    std::vector<VkSemaphore> waitSems;
    std::vector<VkPipelineStageFlags> waitStages;
    std::vector<VkSemaphore> ownedWaits;
    for (auto &pw : m_pendingWaits)
    {
        waitSems.push_back(pw.first);
        waitStages.push_back(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
        if (pw.second)
            ownedWaits.push_back(pw.first);
    }
    m_pendingWaits.clear();

    VkFence fence { VK_NULL_HANDLE };
    VkFenceCreateInfo fi {};
    fi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    if (vkCreateFence(m_device, &fi, nullptr, &fence) != VK_SUCCESS)
        return false;

    VkSubmitInfo submit {};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.waitSemaphoreCount = waitSems.size();
    submit.pWaitSemaphores = waitSems.data();
    submit.pWaitDstStageMask = waitStages.data();
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;

    bool ok { vkQueueSubmit(m_graphicsQueue, 1, &submit, fence) == VK_SUCCESS };
    if (ok)
        vkWaitForFences(m_device, 1, &fence, VK_TRUE, UINT64_MAX);

    vkDestroyFence(m_device, fence, nullptr);
    for (VkSemaphore s : ownedWaits)
        vkDestroySemaphore(m_device, s, nullptr);
    return ok;
}

bool RVKDevice::submitCommandAsync(VkCommandBuffer cmd, VkFence fence, std::vector<VkSemaphore> &ownedWaitsOut,
                                   VkSemaphore signal) const noexcept
{
    std::lock_guard<std::mutex> lock { m_queueMutex };

    std::vector<VkSemaphore> waitSems;
    std::vector<VkPipelineStageFlags> waitStages;
    for (auto &pw : m_pendingWaits)
    {
        waitSems.push_back(pw.first);
        waitStages.push_back(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
        if (pw.second)
            ownedWaitsOut.push_back(pw.first); // caller destroys these once `fence` signals
    }
    m_pendingWaits.clear();

    VkSubmitInfo submit {};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.waitSemaphoreCount = waitSems.size();
    submit.pWaitSemaphores = waitSems.data();
    submit.pWaitDstStageMask = waitStages.data();
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    submit.signalSemaphoreCount = (signal != VK_NULL_HANDLE) ? 1 : 0;
    submit.pSignalSemaphores = (signal != VK_NULL_HANDLE) ? &signal : nullptr;

    return vkQueueSubmit(m_graphicsQueue, 1, &submit, fence) == VK_SUCCESS;
}

void RVKDevice::deferDestroy(VkFence fence, std::function<void()> cleanup) const noexcept
{
    std::lock_guard<std::mutex> lock { m_garbageMutex };
    m_garbage.push_back({ fence, std::move(cleanup), std::this_thread::get_id() });
}

void RVKDevice::queueWait(VkSemaphore semaphore, bool owned) const noexcept
{
    if (semaphore == VK_NULL_HANDLE)
        return;
    std::lock_guard<std::mutex> lock { m_queueMutex };
    m_pendingWaits.emplace_back(semaphore, owned);
}

bool RVKDevice::submitSignal(VkSemaphore semaphore, VkFence fence) const noexcept
{
    std::lock_guard<std::mutex> lock { m_queueMutex };

    VkSubmitInfo submit {};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    // A null semaphore yields a fence-only signal (CPU-side completion tracking).
    submit.signalSemaphoreCount = semaphore != VK_NULL_HANDLE ? 1 : 0;
    submit.pSignalSemaphores = semaphore != VK_NULL_HANDLE ? &semaphore : nullptr;

    return vkQueueSubmit(m_graphicsQueue, 1, &submit, fence) == VK_SUCCESS;
}

bool RVKDevice::submitSignalTimeline(VkSemaphore semaphore, UInt64 value, VkFence fence) const noexcept
{
    std::lock_guard<std::mutex> lock { m_queueMutex };

    VkTimelineSemaphoreSubmitInfo tsi {};
    tsi.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
    tsi.signalSemaphoreValueCount = 1;
    tsi.pSignalSemaphoreValues = &value;

    VkSubmitInfo submit {};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.pNext = &tsi;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &semaphore;

    return vkQueueSubmit(m_graphicsQueue, 1, &submit, fence) == VK_SUCCESS;
}

bool RVKDevice::init() noexcept
{
    return initPropsAndFeatures() &&
           initExtensions() &&
           initDRM() &&
           initQueues() &&
           initDevice() &&
           initProcs() &&
           initSkia() &&
           initFormats();
}

bool RVKDevice::initPropsAndFeatures() noexcept
{
    vkGetPhysicalDeviceProperties(physicalDevice(), &m_properties);
    vkGetPhysicalDeviceFeatures(physicalDevice(), &m_features);
    vkGetPhysicalDeviceMemoryProperties(physicalDevice(), &m_memoryProperties);

    if (m_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU)
    {
        // CPU devices (e.g. llvmpipe/lavapipe) are skipped by default; opt in for a software
        // Vulkan fallback (and to exercise multi-device / cross-device paths).
        const char *allowCpu { std::getenv("CZ_REAM_VK_ALLOW_CPU") };
        if (!allowCpu || atoi(allowCpu) == 0)
        {
            RLog(CZWarning, CZLN, "{}: Unsupported Vulkan device type (CPU). Ignoring it...", m_properties.deviceName);
            return false;
        }
    }

    // Ream's painter uses only vertex/fragment stages; a geometry shader is not required.

    // Score: prefer discrete > integrated > virtual.
    switch (m_properties.deviceType)
    {
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:   m_score = 400; break;
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: m_score = 300; break;
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:    m_score = 200; break;
    default:                                     m_score = 100; break;
    }

    return true;
}

bool RVKDevice::initExtensions() noexcept
{
    UInt32 count { 0 };

    if (vkEnumerateDeviceExtensionProperties(physicalDevice(), nullptr, &count, nullptr) != VK_SUCCESS)
    {
        RLog(CZError, CZLN, "{}: Failed to retrieve device extension count", m_properties.deviceName);
        return false;
    }

    m_extensions.resize(count);

    if (vkEnumerateDeviceExtensionProperties(physicalDevice(), nullptr, &count, m_extensions.data()) != VK_SUCCESS)
    {
        RLog(CZError, CZLN, "{}: Failed to retrieve device extensions", m_properties.deviceName);
        return false;
    }

    auto enable = [this](const char *ext) -> bool
    {
        if (!hasExtension(ext))
            return false;
        m_requiredExtensions.emplace_back(ext);
        return true;
    };

    // The DRM platform must be able to associate the physical device with a DRM node.
    if (m_core.platform() == RPlatform::DRM && !hasExtension(VK_EXT_PHYSICAL_DEVICE_DRM_EXTENSION_NAME))
    {
        RLog(CZError, CZLN, "{}: Missing required device extension {}", m_properties.deviceName, VK_EXT_PHYSICAL_DEVICE_DRM_EXTENSION_NAME);
        return false;
    }
    m_ext.EXT_physical_device_drm = hasExtension(VK_EXT_PHYSICAL_DEVICE_DRM_EXTENSION_NAME);

    if (m_core.platform() == RPlatform::Wayland)
        m_ext.KHR_swapchain = enable(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

    // DMA-buf import/export + DRM format modifier images (the compositor path).
    {
        const bool extMem   { enable(VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME) };
        const bool extMemFd { enable(VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME) };
        m_ext.KHR_external_memory_fd      = extMem && extMemFd;
        m_ext.EXT_external_memory_dma_buf = m_ext.KHR_external_memory_fd && enable(VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME);
    }
    m_ext.KHR_image_format_list         = enable(VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME);
    m_ext.EXT_image_drm_format_modifier = enable(VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME);
    m_ext.EXT_queue_family_foreign      = enable(VK_EXT_QUEUE_FAMILY_FOREIGN_EXTENSION_NAME);

    // Synchronization / interop.
    m_ext.KHR_timeline_semaphore = enable(VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME);
    m_ext.KHR_synchronization2   = enable(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME);
    {
        const bool extSem   { enable(VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME) };
        const bool extSemFd { enable(VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME) };
        m_ext.KHR_external_semaphore_fd = extSem && extSemFd;
    }

    // VK_KHR_dynamic_rendering pulls a dependency chain (depth_stencil_resolve,
    // create_renderpass2, multiview, maintenance2). The painter uses VkRenderPass instead,
    // so it is intentionally not enabled here (revisit in Phase 5 if desired).
    m_ext.KHR_dynamic_rendering = false;

    return true;
}

bool RVKDevice::initDRM() noexcept
{
    if (!m_ext.EXT_physical_device_drm)
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
            // The GBM allocator is best-effort on Wayland; DMA interop still works without it.
            const auto renderDev { makedev(drmProps.renderMajor, drmProps.renderMinor) };
            const auto primaryDev { makedev(drmProps.primaryMajor, drmProps.primaryMinor) };
            const dev_t chosen { drmProps.hasRender ? renderDev : primaryDev };

            drmDevicePtr device;
            if (drmGetDeviceFromDevId(chosen, 0, &device) == 0)
            {
                const int nodeType { drmProps.hasRender ? DRM_NODE_RENDER : DRM_NODE_PRIMARY };
                if (device->available_nodes & (1 << nodeType) && device->nodes[nodeType])
                {
                    m_drmNode = device->nodes[nodeType];
                    m_drmFd = open(m_drmNode.c_str(), O_RDWR | O_CLOEXEC | O_NONBLOCK);
                    if (m_drmFd >= 0)
                    {
                        m_id = chosen;
                        setDRMDriverName(m_drmFd);
                        m_gbmDevice = gbm_create_device(m_drmFd);
                        log = RLog.newWithContext(CZStringUtils::SubStrAfterLastOf(m_drmNode, "/"));
                    }
                }
                drmFreeDevice(&device);
            }
            return true;
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

                    m_gbmDevice = gbm_create_device(m_drmFd);
                    if (!m_gbmDevice)
                        RLog(CZWarning, CZLN, "{}: Failed to create gbm_device", m_properties.deviceName);

                    UInt64 value {};
                    drmGetCap(m_drmFd, DRM_CAP_ADDFB2_MODIFIERS, &value);
                    m_caps.AddFb2Modifiers = value == 1;
                    value = 0;
                    drmGetCap(m_drmFd, DRM_CAP_DUMB_BUFFER, &value);
                    m_caps.DumbBuffer = value == 1;

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

bool RVKDevice::initQueues() noexcept
{
    UInt32 count { 0 };
    vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &count, nullptr);

    std::vector<VkQueueFamilyProperties> families { count };
    vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &count, families.data());

    for (UInt32 i = 0; i < count; i++)
    {
        if (families[i].queueCount > 0 && (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT))
        {
            m_graphicsQueueFamily = i;
            return true;
        }
    }

    RLog(CZError, CZLN, "{}: No graphics-capable queue family found", m_properties.deviceName);
    return false;
}

bool RVKDevice::initDevice() noexcept
{
    const float priority { 1.f };

    VkDeviceQueueCreateInfo queueInfo {};
    queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo.queueFamilyIndex = m_graphicsQueueFamily;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &priority;

    // Build a VkPhysicalDeviceFeatures2 chain and enable only the supported extension features.
    m_features2 = {};
    m_features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;

    void **next { &m_features2.pNext };

    if (m_ext.KHR_timeline_semaphore)
    {
        m_timelineFeatures = {};
        m_timelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES;
        *next = &m_timelineFeatures;
        next = &m_timelineFeatures.pNext;
    }

    if (m_ext.KHR_synchronization2)
    {
        m_sync2Features = {};
        m_sync2Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
        *next = &m_sync2Features;
        next = &m_sync2Features.pNext;
    }

    if (m_ext.KHR_dynamic_rendering)
    {
        m_dynRenderFeatures = {};
        m_dynRenderFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
        *next = &m_dynRenderFeatures;
        next = &m_dynRenderFeatures.pNext;
    }

    // Probe which of the chained features are actually supported (fills the *Features structs),
    // and capture the supported core features to enable.
    vkGetPhysicalDeviceFeatures2(m_physicalDevice, &m_features2);

    // Disable feature flags we don't need but keep the ones required by the backend.
    if (m_ext.KHR_timeline_semaphore && !m_timelineFeatures.timelineSemaphore)
        m_ext.KHR_timeline_semaphore = false;
    if (m_ext.KHR_synchronization2 && !m_sync2Features.synchronization2)
        m_ext.KHR_synchronization2 = false;
    if (m_ext.KHR_dynamic_rendering && !m_dynRenderFeatures.dynamicRendering)
        m_ext.KHR_dynamic_rendering = false;

    VkDeviceCreateInfo createInfo {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pNext = &m_features2; // features2 in pNext => pEnabledFeatures must be null
    createInfo.queueCreateInfoCount = 1;
    createInfo.pQueueCreateInfos = &queueInfo;
    createInfo.enabledExtensionCount = m_requiredExtensions.size();
    createInfo.ppEnabledExtensionNames = m_requiredExtensions.data();

    if (vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device) != VK_SUCCESS)
    {
        RLog(CZError, CZLN, "{}: Failed to create VkDevice", m_properties.deviceName);
        return false;
    }

    vkGetDeviceQueue(m_device, m_graphicsQueueFamily, 0, &m_graphicsQueue);

    VkCommandPoolCreateInfo poolInfo {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    poolInfo.queueFamilyIndex = m_graphicsQueueFamily;

    if (vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_commandPool) != VK_SUCCESS)
    {
        RLog(CZError, CZLN, "{}: Failed to create command pool", m_properties.deviceName);
        return false;
    }

    // Capabilities that depend on the created device.
    m_caps.Rendering = true;
    m_caps.SyncCPU = true;
    m_caps.SyncGPU = true;

    if (m_ext.KHR_timeline_semaphore && drmFd() >= 0)
    {
        UInt64 soCap { 0 }, tlCap { 0 };
        drmGetCap(drmFd(), DRM_CAP_SYNCOBJ, &soCap);
        drmGetCap(drmFd(), DRM_CAP_SYNCOBJ_TIMELINE, &tlCap);
        m_caps.Timeline = soCap == 1 && tlCap == 1;
    }
    else
        m_caps.Timeline = m_ext.KHR_timeline_semaphore;

    // Query ACTUAL sync_file (SYNC_FD) support rather than assuming the extension implies it: NVIDIA
    // advertises VK_KHR_external_semaphore_fd (OPAQUE_FD) but does NOT support SYNC_FD. Getting this
    // wrong is what silently broke synchronization on NVIDIA.
    bool syncFdExport { false }, syncFdImport { false };
    if (m_ext.KHR_external_semaphore_fd)
    {
        VkPhysicalDeviceExternalSemaphoreInfo info {};
        info.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_SEMAPHORE_INFO;
        info.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT;
        VkExternalSemaphoreProperties props {};
        props.sType = VK_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_PROPERTIES;
        vkGetPhysicalDeviceExternalSemaphoreProperties(m_physicalDevice, &info, &props);
        syncFdExport = props.externalSemaphoreFeatures & VK_EXTERNAL_SEMAPHORE_FEATURE_EXPORTABLE_BIT;
        syncFdImport = props.externalSemaphoreFeatures & VK_EXTERNAL_SEMAPHORE_FEATURE_IMPORTABLE_BIT;
    }

    // Export: producing a sync_file works either via SYNC_FD or via a drm_syncobj timeline point.
    m_caps.SyncExport = syncFdExport || m_caps.Timeline;
    // Import: a GPU-side wait on an external sync_file needs SYNC_FD import (importing a drm_syncobj
    // into a VK semaphore is rejected by NVIDIA). Without it, cross-device waits fall back to CPU.
    m_caps.SyncImport = syncFdImport;

    RLog(CZTrace, CZLN, "{}: VkDevice created", m_properties.deviceName);
    return true;
}

bool RVKDevice::initProcs() noexcept
{
    auto load = [this](const char *name) -> PFN_vkVoidFunction
    {
        return vkGetDeviceProcAddr(m_device, name);
    };

    if (m_ext.KHR_external_memory_fd)
    {
        m_procs.getMemoryFdKHR = (PFN_vkGetMemoryFdKHR) load("vkGetMemoryFdKHR");
        m_procs.getMemoryFdPropertiesKHR = (PFN_vkGetMemoryFdPropertiesKHR) load("vkGetMemoryFdPropertiesKHR");
    }

    if (m_ext.EXT_image_drm_format_modifier)
        m_procs.getImageDrmFormatModifierPropertiesEXT = (PFN_vkGetImageDrmFormatModifierPropertiesEXT) load("vkGetImageDrmFormatModifierPropertiesEXT");

    if (m_ext.KHR_external_semaphore_fd)
    {
        m_procs.importSemaphoreFdKHR = (PFN_vkImportSemaphoreFdKHR) load("vkImportSemaphoreFdKHR");
        m_procs.getSemaphoreFdKHR = (PFN_vkGetSemaphoreFdKHR) load("vkGetSemaphoreFdKHR");
    }

    if (m_ext.KHR_timeline_semaphore)
    {
        m_procs.getSemaphoreCounterValueKHR = (PFN_vkGetSemaphoreCounterValueKHR) load("vkGetSemaphoreCounterValueKHR");
        m_procs.waitSemaphoresKHR = (PFN_vkWaitSemaphoresKHR) load("vkWaitSemaphoresKHR");
        m_procs.signalSemaphoreKHR = (PFN_vkSignalSemaphoreKHR) load("vkSignalSemaphoreKHR");
    }

    if (m_ext.KHR_synchronization2)
    {
        m_procs.queueSubmit2KHR = (PFN_vkQueueSubmit2KHR) load("vkQueueSubmit2KHR");
        m_procs.cmdPipelineBarrier2KHR = (PFN_vkCmdPipelineBarrier2KHR) load("vkCmdPipelineBarrier2KHR");
    }

    if (m_ext.KHR_dynamic_rendering)
    {
        m_procs.cmdBeginRenderingKHR = (PFN_vkCmdBeginRenderingKHR) load("vkCmdBeginRenderingKHR");
        m_procs.cmdEndRenderingKHR = (PFN_vkCmdEndRenderingKHR) load("vkCmdEndRenderingKHR");
    }

    return true;
}

bool RVKDevice::initSkia() noexcept
{
    const VkInstance instance { core().instance() };

    m_getProc = [](const char *name, VkInstance i, VkDevice d) -> PFN_vkVoidFunction
    {
        if (d != VK_NULL_HANDLE)
            return vkGetDeviceProcAddr(d, name);
        return vkGetInstanceProcAddr(i, name);
    };

    const auto &instExts { core().enabledInstanceExtensions() };
    m_skExtensions.init(m_getProc, instance, m_physicalDevice,
                        instExts.size(), instExts.data(),
                        m_requiredExtensions.size(), m_requiredExtensions.data());

    skgpu::VulkanBackendContext bc {};
    bc.fInstance = instance;
    bc.fPhysicalDevice = m_physicalDevice;
    bc.fDevice = m_device;
    bc.fQueue = m_graphicsQueue;
    bc.fGraphicsQueueIndex = m_graphicsQueueFamily;
    bc.fMaxAPIVersion = VK_API_VERSION_1_1;
    bc.fVkExtensions = &m_skExtensions;
    bc.fDeviceFeatures2 = &m_features2;
    bc.fGetProc = m_getProc;

    m_allocator = skgpu::VulkanMemoryAllocators::Make(bc, skgpu::ThreadSafe::kYes, std::nullopt);

    if (!m_allocator)
    {
        RLog(CZError, CZLN, "{}: Failed to create Vulkan memory allocator", m_properties.deviceName);
        return false;
    }

    // The Skia GrDirectContext is created lazily on first use (skContext()). This avoids paying
    // for it on devices used only for the native painter / DMA transfers, and avoids initializing
    // Skia's Vulkan caps on devices where that is problematic (e.g. some software implementations).
    return true;
}

sk_sp<GrDirectContext> RVKDevice::makeSkContext() const noexcept
{
    skgpu::VulkanBackendContext bc {};
    bc.fInstance = core().instance();
    bc.fPhysicalDevice = m_physicalDevice;
    bc.fDevice = m_device;
    bc.fQueue = m_graphicsQueue;
    bc.fGraphicsQueueIndex = m_graphicsQueueFamily;
    bc.fMaxAPIVersion = VK_API_VERSION_1_1;
    bc.fVkExtensions = &m_skExtensions;
    bc.fDeviceFeatures2 = &m_features2;
    bc.fMemoryAllocator = m_allocator;
    bc.fGetProc = m_getProc;

    return GrDirectContexts::MakeVulkan(bc, CZ::GetSKContextOptions());
}

sk_sp<GrDirectContext> RVKDevice::skContext() const noexcept
{
    if (std::this_thread::get_id() == m_mainThread)
    {
        if (!m_skContext)
            m_skContext = makeSkContext();
        return m_skContext;
    }

    std::lock_guard<std::mutex> lock { m_skContextsMutex };
    auto &ctx { m_skContexts[std::this_thread::get_id()] };
    if (!ctx)
        ctx = makeSkContext();
    return ctx;
}

bool RVKDevice::initFormats() noexcept
{
    const auto probeNative = [this](RFormat drm, VkFormat vk)
    {
        VkFormatProperties props {};
        vkGetPhysicalDeviceFormatProperties(m_physicalDevice, vk, &props);

        const bool optSampled { (props.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) != 0 };
        const bool optColor   { (props.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) != 0 };

        if (optSampled)
            m_textureFormats.add(drm, DRM_FORMAT_MOD_INVALID);
        if (optColor)
            m_renderFormats.add(drm, DRM_FORMAT_MOD_INVALID);
    };

    const auto probeModifiers = [this](RFormat drm, VkFormat vk)
    {
        VkDrmFormatModifierPropertiesListEXT modList {};
        modList.sType = VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT;

        VkFormatProperties2 props2 {};
        props2.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2;
        props2.pNext = &modList;

        vkGetPhysicalDeviceFormatProperties2(m_physicalDevice, vk, &props2);

        if (modList.drmFormatModifierCount == 0)
            return;

        std::vector<VkDrmFormatModifierPropertiesEXT> mods { modList.drmFormatModifierCount };
        modList.pDrmFormatModifierProperties = mods.data();
        vkGetPhysicalDeviceFormatProperties2(m_physicalDevice, vk, &props2);

        for (const auto &mod : mods)
        {
            const bool sampled { (mod.drmFormatModifierTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) != 0 };
            const bool color   { (mod.drmFormatModifierTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) != 0 };

            if (sampled)
                m_dmaTextureFormats.add(drm, mod.drmFormatModifier);
            if (color)
                m_dmaRenderFormats.add(drm, mod.drmFormatModifier);
        }
    };

    for (RFormat drm : RDRMFormat::SupportedFormats())
    {
        const VkFormat vk { RVKFormat::FromDRM(drm) };
        if (vk == VK_FORMAT_UNDEFINED)
            continue;

        probeNative(drm, vk);

        if (m_ext.EXT_image_drm_format_modifier)
            probeModifiers(drm, vk);
    }

    // DMA formats are a subset available for import/export; fold them into the generic sets.
    m_textureFormats = RDRMFormatSet::Union(m_textureFormats, m_dmaTextureFormats);
    m_renderFormats = RDRMFormatSet::Union(m_renderFormats, m_dmaRenderFormats);

    const char *env { std::getenv("CZ_REAM_VK_LOG_LEVEL") };
    if (env && static_cast<UInt32>(atoi(env)) >= CZTrace)
    {
        log(CZDebug, "VK DMA Texture Formats:");
        m_dmaTextureFormats.log();
        log(CZDebug, "VK DMA Render Formats:");
        m_dmaRenderFormats.log();
    }

    return true;
}
