#include <wayland-client.h>
#define VK_USE_PLATFORM_WAYLAND_KHR
#include <vulkan/vulkan.h>

#include <CZ/Ream/VK/RVKSwapchainWL.h>
#include <CZ/Ream/VK/RVKCore.h>
#include <CZ/Ream/VK/RVKDevice.h>
#include <CZ/Ream/VK/RVKImage.h>
#include <CZ/Ream/WL/RWLPlatformHandle.h>
#include <CZ/Ream/RCore.h>
#include <CZ/Ream/RLog.h>

#include <algorithm>

using namespace CZ;

RVKSwapchainWL::RVKSwapchainWL(std::shared_ptr<RVKCore> core, RVKDevice *device, wl_surface *surface, SkISize size) noexcept :
    RWLSwapchain(size, surface),
    m_core(core),
    m_device(device)
{}

std::shared_ptr<RVKSwapchainWL> RVKSwapchainWL::Make(wl_surface *surface, SkISize size) noexcept
{
    auto core { RCore::Get() };
    auto vkCore { core ? core->asVK() : nullptr };
    if (!vkCore)
        return {};

    if (core->platform() != RPlatform::Wayland)
    {
        RLog(CZError, CZLN, "RVKSwapchainWL requires the Wayland platform");
        return {};
    }

    auto *wl { core->options().platformHandle->asWL() };
    if (!wl || !wl->wlDisplay())
    {
        RLog(CZError, CZLN, "RVKSwapchainWL: missing wl_display");
        return {};
    }

    RVKDevice *device { vkCore->mainDevice() };
    auto sc { std::shared_ptr<RVKSwapchainWL>(new RVKSwapchainWL(vkCore, device, surface, size)) };

    VkWaylandSurfaceCreateInfoKHR si {};
    si.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
    si.display = wl->wlDisplay();
    si.surface = surface;
    if (vkCreateWaylandSurfaceKHR(vkCore->instance(), &si, nullptr, &sc->m_vkSurface) != VK_SUCCESS)
    {
        RLog(CZError, CZLN, "RVKSwapchainWL: vkCreateWaylandSurfaceKHR failed");
        return {};
    }

    VkBool32 supported { VK_FALSE };
    vkGetPhysicalDeviceSurfaceSupportKHR(device->physicalDevice(), device->graphicsQueueFamily(), sc->m_vkSurface, &supported);
    if (!supported)
    {
        RLog(CZError, CZLN, "RVKSwapchainWL: graphics queue does not support presentation");
        return {};
    }

    VkFenceCreateInfo fi {};
    fi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    if (vkCreateFence(device->device(), &fi, nullptr, &sc->m_acquireFence) != VK_SUCCESS)
        return {};

    if (!sc->createSwapchain(size, VK_NULL_HANDLE))
        return {};

    return sc;
}

bool RVKSwapchainWL::createSwapchain(SkISize size, VkSwapchainKHR oldSwapchain) noexcept
{
    const VkPhysicalDevice phys { m_device->physicalDevice() };

    VkSurfaceCapabilitiesKHR caps {};
    if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phys, m_vkSurface, &caps) != VK_SUCCESS)
        return false;

    // Pick a format (prefer B8G8R8A8_UNORM).
    UInt32 fmtCount { 0 };
    vkGetPhysicalDeviceSurfaceFormatsKHR(phys, m_vkSurface, &fmtCount, nullptr);
    if (fmtCount == 0)
        return false;
    std::vector<VkSurfaceFormatKHR> formats { fmtCount };
    vkGetPhysicalDeviceSurfaceFormatsKHR(phys, m_vkSurface, &fmtCount, formats.data());

    VkSurfaceFormatKHR chosen { formats[0] };
    for (const auto &f : formats)
        if (f.format == VK_FORMAT_B8G8R8A8_UNORM && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            chosen = f;
            break;
        }
    m_format = chosen.format;
    m_colorSpace = chosen.colorSpace;

    m_extent.width = (caps.currentExtent.width != 0xFFFFFFFF)
        ? caps.currentExtent.width
        : std::clamp((UInt32)size.width(), caps.minImageExtent.width, caps.maxImageExtent.width);
    m_extent.height = (caps.currentExtent.height != 0xFFFFFFFF)
        ? caps.currentExtent.height
        : std::clamp((UInt32)size.height(), caps.minImageExtent.height, caps.maxImageExtent.height);

    // Triple-buffer: with only 2 images under FIFO, the single free image stays held by the
    // compositor until the next vblank, so the per-frame blocking acquire (vkWaitForFences below)
    // stalls the client's event loop every frame. A third image means acquire almost always finds
    // a free image and returns immediately, keeping the loop responsive.
    UInt32 minCount { std::max(caps.minImageCount, 3u) };
    if (caps.maxImageCount > 0)
        minCount = std::min(minCount, caps.maxImageCount);

    VkSwapchainCreateInfoKHR ci {};
    ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    ci.surface = m_vkSurface;
    ci.minImageCount = minCount;
    ci.imageFormat = m_format;
    ci.imageColorSpace = m_colorSpace;
    ci.imageExtent = m_extent;
    ci.imageArrayLayers = 1;
    ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ci.preTransform = (caps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
        ? VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR : caps.currentTransform;

    // Honor the window buffer's alpha so translucent content (e.g. CSD drop shadows) blends
    // with the compositor. VK_COMPOSITE_ALPHA_OPAQUE ignores alpha, turning premultiplied
    // shadows (0,0,0,a) into solid black. Ream/Skia render premultiplied, so prefer that.
    const VkCompositeAlphaFlagBitsKHR alphaPrefs[] {
        VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
        VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
    };
    ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    for (auto a : alphaPrefs)
        if (caps.supportedCompositeAlpha & a) { ci.compositeAlpha = a; break; }
    RLog(CZDebug, CZLN, "RVKSwapchainWL: compositeAlpha=0x{:x} (supported=0x{:x})",
         (unsigned)ci.compositeAlpha, (unsigned)caps.supportedCompositeAlpha);
    ci.presentMode = VK_PRESENT_MODE_FIFO_KHR; // always supported
    ci.clipped = VK_TRUE;
    ci.oldSwapchain = oldSwapchain;

    if (vkCreateSwapchainKHR(m_device->device(), &ci, nullptr, &m_swapchain) != VK_SUCCESS)
    {
        RLog(CZError, CZLN, "RVKSwapchainWL: vkCreateSwapchainKHR failed");
        return false;
    }

    UInt32 imgCount { 0 };
    vkGetSwapchainImagesKHR(m_device->device(), m_swapchain, &imgCount, nullptr);
    std::vector<VkImage> images { imgCount };
    vkGetSwapchainImagesKHR(m_device->device(), m_swapchain, &imgCount, images.data());

    if (m_presentPool == VK_NULL_HANDLE)
    {
        VkCommandPoolCreateInfo pci {};
        pci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pci.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        pci.queueFamilyIndex = m_device->graphicsQueueFamily();
        if (vkCreateCommandPool(m_device->device(), &pci, nullptr, &m_presentPool) != VK_SUCCESS)
        {
            RLog(CZError, CZLN, "RVKSwapchainWL: failed to create present command pool");
            return false;
        }
    }

    m_buffers.clear();
    m_buffers.resize(imgCount);
    for (UInt32 i = 0; i < imgCount; i++)
    {
        m_buffers[i].image = RVKImage::WrapVkImage(m_device, images[i], m_format,
            SkISize::Make(m_extent.width, m_extent.height), ci.imageUsage, false);
        if (!m_buffers[i].image)
        {
            RLog(CZError, CZLN, "RVKSwapchainWL: failed to wrap swapchain image");
            return false;
        }

        VkSemaphoreCreateInfo sci {};
        sci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        if (vkCreateSemaphore(m_device->device(), &sci, nullptr, &m_buffers[i].presentSem) != VK_SUCCESS)
        {
            RLog(CZError, CZLN, "RVKSwapchainWL: failed to create present semaphore");
            return false;
        }
    }

    m_size = SkISize::Make(m_extent.width, m_extent.height);
    return true;
}

void RVKSwapchainWL::destroySwapchain() noexcept
{
    m_device->wait();
    m_device->clearGarbage(); // free any deferred present-transition command buffers
    for (auto &buf : m_buffers)
        if (buf.presentSem != VK_NULL_HANDLE)
            vkDestroySemaphore(m_device->device(), buf.presentSem, nullptr);
    m_buffers.clear(); // destroys wrapped RVKImages (views)
    if (m_presentPool != VK_NULL_HANDLE)
    {
        vkDestroyCommandPool(m_device->device(), m_presentPool, nullptr);
        m_presentPool = VK_NULL_HANDLE;
    }
    if (m_swapchain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(m_device->device(), m_swapchain, nullptr);
        m_swapchain = VK_NULL_HANDLE;
    }
}

std::optional<const RSwapchainImage> RVKSwapchainWL::acquire() noexcept
{
    if (m_acquired)
    {
        RLog(CZError, CZLN, "RVKSwapchainWL::acquire called twice without present");
        return std::nullopt;
    }

    UInt32 index { 0 };
    VkResult r { vkAcquireNextImageKHR(m_device->device(), m_swapchain, UINT64_MAX, VK_NULL_HANDLE, m_acquireFence, &index) };

    if (r == VK_ERROR_OUT_OF_DATE_KHR)
    {
        // Recreate at the current size and retry once.
        if (!resize(m_size))
            return std::nullopt;
        r = vkAcquireNextImageKHR(m_device->device(), m_swapchain, UINT64_MAX, VK_NULL_HANDLE, m_acquireFence, &index);
    }

    if (r != VK_SUCCESS && r != VK_SUBOPTIMAL_KHR)
        return std::nullopt;

    // Blocking wait so the image is ready for either the painter or Skia to render into.
    vkWaitForFences(m_device->device(), 1, &m_acquireFence, VK_TRUE, UINT64_MAX);
    vkResetFences(m_device->device(), 1, &m_acquireFence);

    auto &buf { m_buffers[index] };
    const UInt32 age { buf.used ? (m_frame - buf.lastFrame) : 0 };

    // A freshly acquired swapchain image has undefined content.
    buf.image->setLayout(VK_IMAGE_LAYOUT_UNDEFINED);

    m_acquired = true;
    m_currentIndex = index;

    RSwapchainImage out {};
    out.image = buf.image;
    out.age = age;
    out.frame = m_frame;
    out.index = index;
    return out;
}

bool RVKSwapchainWL::present(const RSwapchainImage &image, SkRegion *) noexcept
{
    if (!m_acquired)
        return false;

    auto &buf { m_buffers[m_currentIndex] };
    const VkDevice dev { m_device->device() };

    // Transition the rendered image into PRESENT_SRC and submit it WITHOUT blocking: the submit is
    // queue-ordered after the render work and signals buf.presentSem, which vkQueuePresentKHR waits
    // on. The transition command buffer is reclaimed by the fence-tracked GC (clearGarbage), so the
    // CPU never stalls on the frame's GPU work and can run ahead into the next frame.
    VkCommandBuffer cmd { VK_NULL_HANDLE };
    VkCommandBufferAllocateInfo ai {};
    ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.commandPool = m_presentPool;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = 1;

    VkResult r { VK_SUCCESS };
    bool async { false };
    VkFence fence { VK_NULL_HANDLE };

    if (vkAllocateCommandBuffers(dev, &ai, &cmd) == VK_SUCCESS)
    {
        VkCommandBufferBeginInfo bi {};
        bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmd, &bi);
        buf.image->transitionLayout(cmd, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT, 0);
        vkEndCommandBuffer(cmd);

        VkFenceCreateInfo fi {};
        fi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        if (vkCreateFence(dev, &fi, nullptr, &fence) == VK_SUCCESS)
        {
            std::vector<VkSemaphore> ownedWaits;
            if (m_device->submitCommandAsync(cmd, fence, ownedWaits, buf.presentSem))
            {
                async = true;
                RVKDevice *device { m_device };
                VkCommandPool pool { m_presentPool };
                device->deferDestroy(fence, [device, pool, cmd, ow = std::move(ownedWaits)]()
                {
                    vkFreeCommandBuffers(device->device(), pool, 1, &cmd);
                    for (VkSemaphore s : ow) vkDestroySemaphore(device->device(), s, nullptr);
                });
            }
            else
                vkDestroyFence(dev, fence, nullptr);
        }
    }

    if (!async)
    {
        // Fallback: blocking transition (also frees cmd if it was allocated).
        m_device->immediateSubmit([&](VkCommandBuffer c)
        {
            buf.image->transitionLayout(c, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT, 0);
        });
        if (cmd != VK_NULL_HANDLE)
            vkFreeCommandBuffers(dev, m_presentPool, 1, &cmd);
    }

    VkPresentInfoKHR pi {};
    pi.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    pi.waitSemaphoreCount = async ? 1 : 0;
    pi.pWaitSemaphores = async ? &buf.presentSem : nullptr;
    pi.swapchainCount = 1;
    pi.pSwapchains = &m_swapchain;
    pi.pImageIndices = &m_currentIndex;

    {
        std::lock_guard<std::mutex> lock { m_device->queueMutex() };
        r = vkQueuePresentKHR(m_device->graphicsQueue(), &pi);
    }

    buf.used = true;
    buf.lastFrame = m_frame;
    m_frame++;
    m_acquired = false;

    if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR)
        resize(m_size);
    else if (r != VK_SUCCESS)
        return false;

    CZ_UNUSED(image)
    return true;
}

bool RVKSwapchainWL::resize(SkISize size) noexcept
{
    VkSwapchainKHR old { m_swapchain };
    m_device->wait();
    m_device->clearGarbage(); // free deferred present-transition command buffers
    for (auto &buf : m_buffers)
        if (buf.presentSem != VK_NULL_HANDLE)
            vkDestroySemaphore(m_device->device(), buf.presentSem, nullptr);
    m_buffers.clear();
    m_swapchain = VK_NULL_HANDLE;

    const bool ok { createSwapchain(size, old) };

    if (old != VK_NULL_HANDLE)
        vkDestroySwapchainKHR(m_device->device(), old, nullptr);

    m_acquired = false;
    return ok;
}

RVKSwapchainWL::~RVKSwapchainWL() noexcept
{
    destroySwapchain();

    if (m_acquireFence != VK_NULL_HANDLE)
        vkDestroyFence(m_device->device(), m_acquireFence, nullptr);
    if (m_vkSurface != VK_NULL_HANDLE)
        vkDestroySurfaceKHR(m_core->instance(), m_vkSurface, nullptr);
}
