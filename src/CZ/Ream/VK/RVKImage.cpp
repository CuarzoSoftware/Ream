#include <CZ/Ream/VK/RVKImage.h>
#include <CZ/Ream/VK/RVKDevice.h>
#include <CZ/Ream/VK/RVKCore.h>
#include <CZ/Ream/VK/RVKFormat.h>
#include <CZ/Ream/SK/RSKFormat.h>
#include <CZ/Ream/DRM/RDRMFormat.h>
#include <CZ/Ream/DRM/RDRMFramebuffer.h>
#include <CZ/Ream/GBM/RGBMBo.h>
#include <CZ/Ream/RCore.h>
#include <CZ/Ream/RLog.h>
#include <CZ/Core/CZBitset.h>

#include <unistd.h>
#include <sys/stat.h>

#include <CZ/skia/core/SkColorSpace.h>
#include <CZ/skia/gpu/ganesh/GrDirectContext.h>
#include <CZ/skia/gpu/ganesh/vk/GrVkTypes.h>
#include <CZ/skia/gpu/ganesh/vk/GrVkBackendSurface.h>
#include <CZ/skia/gpu/ganesh/SkImageGanesh.h>
#include <CZ/skia/gpu/ganesh/SkSurfaceGanesh.h>

#include <drm_fourcc.h>
#include <cstring>

using namespace CZ;

// Mirrors RGLImage's ValidateConstraints: the image must satisfy every requested per-device
// cap and at least one required read/write format. Crucial for SRM's swapchain strategy
// fallback (e.g. Prime requires RImageCap_Src on a *different* device than the allocator,
// which single-device VK images can't provide, so Make must fail and let SRM fall back).
static bool ValidateConstraints(const std::shared_ptr<RVKImage> &image, const RImageConstraints *constraints) noexcept
{
    if (!constraints)
        return true;

    bool hasReadFormat { constraints->readFormats.empty() };
    for (auto fmt : constraints->readFormats)
        if (image->readFormats().contains(fmt)) { hasReadFormat = true; break; }
    if (!hasReadFormat)
        return false;

    bool hasWriteFormat { constraints->writeFormats.empty() };
    for (auto fmt : constraints->writeFormats)
        if (image->writeFormats().contains(fmt)) { hasWriteFormat = true; break; }
    if (!hasWriteFormat)
        return false;

    for (const auto &dev : constraints->caps)
        if (dev.first && dev.second != image->checkDeviceCaps(dev.second, dev.first))
            return false;

    return true;
}

RVKImage::RVKImage(std::shared_ptr<RCore> core, RVKDevice *device, SkISize size,
                   const RFormatInfo *formatInfo, SkAlphaType alphaType, RModifier modifier) noexcept :
    RImage(core, device, size, formatInfo, alphaType, modifier),
    m_dev(device)
{}

RVKImage::~RVKImage() noexcept
{
    if (!m_dev || m_dev->device() == VK_NULL_HANDLE)
        return;

    // Skia records draw ops lazily; a wrapped SkSurface/SkImage may still have deferred work
    // referencing this (borrowed) VkImage. Flush+submit+finish so nothing references it, then
    // drop the views before destroying the image.
    // NOTE: single-thread safe. Cross-thread destruction of images with views built on another
    // thread must go through the Phase 6 fence-tracked garbage collector.
    if (m_hasBackendTexture || m_hasBackendRT)
    {
        if (auto ctx { m_dev->skContext() })
            ctx->flushAndSubmit(GrSyncCpu::kYes);
    }

    {
        std::lock_guard<std::mutex> lock { m_mutex };
        m_views.clear();
    }
    m_backendTexture = {};
    m_backendRT = {};

    // Ensure no in-flight GPU work references the image before destroying it, then drain the
    // deferred-destruction queue so any framebuffer/resources referencing this image's view (from
    // an async painter pass) are freed BEFORE the view/image itself is destroyed.
    m_dev->wait();
    m_dev->clearGarbage();

    const VkDevice dev { m_dev->device() };

    if (m_view != VK_NULL_HANDLE)
        vkDestroyImageView(dev, m_view, nullptr);

    if (m_image != VK_NULL_HANDLE && m_ownsImage)
        vkDestroyImage(dev, m_image, nullptr);

    if (m_memory != VK_NULL_HANDLE && m_ownsMemory)
        vkFreeMemory(dev, m_memory, nullptr);

    // Destroy cross-device imported copies (each on its own device).
    for (auto &[sdev, s] : m_shared)
    {
        if (!sdev || sdev->device() == VK_NULL_HANDLE)
            continue;
        sdev->wait();
        if (s.view != VK_NULL_HANDLE) vkDestroyImageView(sdev->device(), s.view, nullptr);
        if (s.image != VK_NULL_HANDLE) vkDestroyImage(sdev->device(), s.image, nullptr);
        if (s.memory != VK_NULL_HANDLE) vkFreeMemory(sdev->device(), s.memory, nullptr);
    }
    m_shared.clear();
}

std::shared_ptr<RVKImage> RVKImage::Make(SkISize size, const RDRMFormat &format, const RImageConstraints *constraints) noexcept
{
    auto core { RCore::Get() };
    if (!core)
    {
        RLog(CZError, CZLN, "Missing RCore");
        return {};
    }

    RDevice *dev { (constraints && constraints->allocator) ? constraints->allocator : core->mainDevice() };
    RVKDevice *vk { dev ? dev->asVK() : nullptr };
    if (!vk)
    {
        RLog(CZError, CZLN, "RVKImage::Make: invalid allocator device");
        return {};
    }

    if (size.isEmpty())
    {
        RLog(CZError, CZLN, "RVKImage::Make: empty size");
        return {};
    }

    const RFormatInfo *fi { RDRMFormat::GetInfo(format.format()) };
    if (!fi)
    {
        RLog(CZError, CZLN, "RVKImage::Make: unsupported DRM format {}", RDRMFormat::FormatName(format.format()));
        return {};
    }

    const VkFormat vkFmt { RVKFormat::FromDRM(format.format()) };
    if (vkFmt == VK_FORMAT_UNDEFINED)
    {
        RLog(CZError, CZLN, "RVKImage::Make: no VkFormat for DRM format {}", RDRMFormat::FormatName(format.format()));
        return {};
    }

    if (fi->pixelsPerBlock() != 1)
    {
        RLog(CZError, CZLN, "RVKImage::Make: block formats are not supported by native storage");
        return {};
    }

    // GBM-backed (dma-shareable) storage is required for: scanout (DRMFb/GBMBo caps), and
    // cross-device use (caps requested for a device other than the allocator, e.g. SRM Prime,
    // which renders on one GPU and samples on another). Otherwise use native optimal storage.
    bool needShared { false };
    if (constraints)
    {
        const auto it { constraints->caps.find(vk) };
        if (it != constraints->caps.end() &&
            (it->second.has(RImageCap_DRMFb) || it->second.has(RImageCap_GBMBo)))
            needShared = true;

        for (const auto &c : constraints->caps)
            if (c.first && c.first != vk && !c.second.isEmpty())
                needShared = true; // a non-allocator device needs to use this image
    }

    if (needShared && (!vk->extensions().EXT_image_drm_format_modifier || !vk->extensions().EXT_external_memory_dma_buf))
    {
        RLog(CZDebug, CZLN, "RVKImage::Make: shared/scanout storage requested but DMA extensions unavailable");
        return {};
    }

    const SkAlphaType alphaType { fi->alpha ? kPremul_SkAlphaType : kOpaque_SkAlphaType };

    auto img { std::shared_ptr<RVKImage>(new RVKImage(core, vk, size, fi, alphaType, DRM_FORMAT_MOD_INVALID)) };
    img->m_self = img;
    img->m_vkFormat = vkFmt;

    if (needShared)
    {
        if (!img->initGBMStorage(format))
            return {};
    }
    else if (!img->initNativeStorage())
        return {};

    img->assignReadWriteFormats();

    if (!ValidateConstraints(img, constraints))
    {
        RLog(CZDebug, CZLN, "RVKImage::Make: constraints not satisfied");
        return {};
    }

    return img;
}

std::shared_ptr<RVKImage> RVKImage::FromDMA(const RDMABufferInfo &info, CZOwn ownership, const RImageConstraints *constraints) noexcept
{
    auto fail = [&]() -> std::shared_ptr<RVKImage>
    {
        if (ownership == CZOwn::Own)
        {
            auto copy { info };
            copy.closeFds();
        }
        return {};
    };

    auto core { RCore::Get() };
    if (!core) { RLog(CZError, CZLN, "Missing RCore"); return fail(); }

    if (!info.isValid())
        return fail();

    RDevice *dev { (constraints && constraints->allocator) ? constraints->allocator : core->mainDevice() };
    RVKDevice *vk { dev ? dev->asVK() : nullptr };
    if (!vk) { RLog(CZError, CZLN, "RVKImage::FromDMA: invalid allocator"); return fail(); }

    if (!vk->extensions().EXT_image_drm_format_modifier || !vk->extensions().EXT_external_memory_dma_buf)
    {
        RLog(CZError, CZLN, "RVKImage::FromDMA: DMA/DRM-modifier extensions unavailable");
        return fail();
    }

    const RFormatInfo *fi { RDRMFormat::GetInfo(info.format) };
    const VkFormat vkFmt { RVKFormat::FromDRM(info.format) };
    if (!fi || vkFmt == VK_FORMAT_UNDEFINED)
    {
        RLog(CZError, CZLN, "RVKImage::FromDMA: unsupported format {}", RDRMFormat::FormatName(info.format));
        return fail();
    }

    const SkAlphaType alphaType { fi->alpha ? kPremul_SkAlphaType : kOpaque_SkAlphaType };

    auto img { std::shared_ptr<RVKImage>(new RVKImage(core, vk, SkISize::Make(info.width, info.height), fi, alphaType, info.modifier)) };
    img->m_self = img;
    img->m_vkFormat = vkFmt;

    if (!img->importDMA(info, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT))
        return fail();

    // Keep a GEM-handle-backed bo for DRM framebuffer / cross-device interop (mirrors RGLImage).
    img->m_bo = RGBMBo::MakeFromDMA(info, vk);
    img->m_exportable = (img->m_bo != nullptr);

    img->assignReadWriteFormats();

    if (!ValidateConstraints(img, constraints))
    {
        RLog(CZDebug, CZLN, "RVKImage::FromDMA: constraints not satisfied");
        return fail();
    }

    if (ownership == CZOwn::Own)
    {
        auto copy { info };
        copy.closeFds();
    }

    return img;
}

std::shared_ptr<RVKImage> RVKImage::WrapVkImage(RVKDevice *device, VkImage image, VkFormat format,
                                                SkISize size, VkImageUsageFlags usage, bool alpha) noexcept
{
    auto core { RCore::Get() };
    if (!core || !device || image == VK_NULL_HANDLE || size.isEmpty())
        return {};

    const RFormat drm { RVKFormat::ToDRM(format) };
    const RFormatInfo *fi { RDRMFormat::GetInfo(drm) };
    if (!fi)
    {
        RLog(CZError, CZLN, "RVKImage::WrapVkImage: no DRM format for VkFormat {}", (int)format);
        return {};
    }

    const SkAlphaType alphaType { alpha ? kPremul_SkAlphaType : kOpaque_SkAlphaType };
    auto img { std::shared_ptr<RVKImage>(new RVKImage(core, device, size, fi, alphaType, DRM_FORMAT_MOD_INVALID)) };
    img->m_self = img;
    img->m_vkFormat = format;
    img->m_image = image;
    img->m_ownsImage = false;
    img->m_ownsMemory = false;
    img->m_usage = usage;
    img->m_tiling = VK_IMAGE_TILING_OPTIMAL;

    VkImageViewCreateInfo vi {};
    vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vi.image = image;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format = format;
    vi.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    if (vkCreateImageView(device->device(), &vi, nullptr, &img->m_view) != VK_SUCCESS)
    {
        RLog(CZError, CZLN, "RVKImage::WrapVkImage: vkCreateImageView failed");
        return {};
    }

    img->assignReadWriteFormats();
    return img;
}

bool RVKImage::initGBMStorage(const RDRMFormat &format) noexcept
{
    auto bo { RGBMBo::Make(size(), format, m_dev) };
    if (!bo)
    {
        RLog(CZError, CZLN, "RVKImage: RGBMBo::Make failed");
        return false;
    }

    auto dma { bo->dmaExport() };
    if (!dma)
    {
        RLog(CZError, CZLN, "RVKImage: dmaExport failed");
        return false;
    }

    m_bo = bo;
    m_modifier = bo->modifier();
    m_exportable = true; // GBM-backed storage is dma-shareable across devices

    const bool ok { importDMA(*dma, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                                    VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT) };
    dma->closeFds();
    return ok;
}

// Imports a dma-buf as a VkImage (+view) on an arbitrary device, via VK_EXT_image_drm_format_modifier
// + external memory. Used both for the allocator's own storage and for cross-device sampling (Prime).
//
// Supports multi-plane buffers whose planes reside in a single dma-buf memory object (the common
// case: e.g. compression-metadata planes from tiled+CCS modifiers). Buffers with planes in separate
// memory objects (disjoint, typical of some YUV layouts) additionally require per-plane binding and
// a Y'CbCr sampler conversion; those are not supported here (and their YUV formats are not mapped by
// RVKFormat), so they are rejected cleanly.
bool RVKImage::ImportDMA(RVKDevice *dev, const RDMABufferInfo &info, VkFormat vkFmt, VkImageUsageFlags usage,
                         VkImage &outImage, VkDeviceMemory &outMemory, VkImageView &outView, VkDeviceSize *outSize) noexcept
{
    if (info.planeCount < 1 || info.planeCount > 4)
    {
        RLog(CZError, CZLN, "RVKImage::ImportDMA: invalid plane count {}", info.planeCount);
        return false;
    }

    // Detect disjoint buffers (planes backed by distinct dma-buf memory objects). Duplicated fds of
    // one buffer share an inode; distinct buffers do not.
    if (info.planeCount > 1)
    {
        struct stat st0 {};
        if (fstat(info.fd[0], &st0) != 0)
        {
            RLog(CZError, CZLN, "RVKImage::ImportDMA: fstat failed on plane 0 fd");
            return false;
        }
        for (int i = 1; i < info.planeCount; i++)
        {
            struct stat st {};
            if (fstat(info.fd[i], &st) == 0 && (st.st_ino != st0.st_ino || st.st_dev != st0.st_dev))
            {
                RLog(CZError, CZLN, "RVKImage::ImportDMA: disjoint multi-plane buffers "
                    "(separate memory per plane) are not supported");
                return false;
            }
        }
    }

    const VkDevice vkDev { dev->device() };

    VkExternalMemoryImageCreateInfo extImg {};
    extImg.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
    extImg.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;

    // One layout entry per memory plane of the modifier (offset/stride within the shared buffer).
    VkSubresourceLayout planeLayouts[4] {};
    for (int i = 0; i < info.planeCount; i++)
    {
        planeLayouts[i].offset = info.offset[i];
        planeLayouts[i].rowPitch = info.stride[i];
    }

    VkImageDrmFormatModifierExplicitCreateInfoEXT modInfo {};
    modInfo.sType = VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_EXPLICIT_CREATE_INFO_EXT;
    modInfo.drmFormatModifier = info.modifier;
    modInfo.drmFormatModifierPlaneCount = (UInt32)info.planeCount;
    modInfo.pPlaneLayouts = planeLayouts;
    modInfo.pNext = &extImg;

    VkImageCreateInfo ci {};
    ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ci.pNext = &modInfo;
    ci.imageType = VK_IMAGE_TYPE_2D;
    ci.format = vkFmt;
    ci.extent = { (UInt32)info.width, (UInt32)info.height, 1 };
    ci.mipLevels = 1;
    ci.arrayLayers = 1;
    ci.samples = VK_SAMPLE_COUNT_1_BIT;
    ci.tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT;
    ci.usage = usage;
    ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkImage image { VK_NULL_HANDLE };
    VkDeviceMemory memory { VK_NULL_HANDLE };
    VkImageView view { VK_NULL_HANDLE };

    auto fail = [&]() -> bool
    {
        if (view != VK_NULL_HANDLE) vkDestroyImageView(vkDev, view, nullptr);
        if (memory != VK_NULL_HANDLE) vkFreeMemory(vkDev, memory, nullptr);
        if (image != VK_NULL_HANDLE) vkDestroyImage(vkDev, image, nullptr);
        return false;
    };

    if (vkCreateImage(vkDev, &ci, nullptr, &image) != VK_SUCCESS)
    {
        RLog(CZError, CZLN, "RVKImage::ImportDMA: vkCreateImage failed");
        return false;
    }

    // vkAllocateMemory takes ownership of the fd on success, so hand it a dup.
    const int impFd { fcntl(info.fd[0], F_DUPFD_CLOEXEC, 0) };
    if (impFd < 0)
    {
        RLog(CZError, CZLN, "RVKImage::ImportDMA: failed to dup dma-buf fd");
        return fail();
    }

    VkMemoryFdPropertiesKHR fdProps {};
    fdProps.sType = VK_STRUCTURE_TYPE_MEMORY_FD_PROPERTIES_KHR;
    if (!dev->procs().getMemoryFdPropertiesKHR ||
        dev->procs().getMemoryFdPropertiesKHR(vkDev, VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT, impFd, &fdProps) != VK_SUCCESS)
    {
        RLog(CZError, CZLN, "RVKImage::ImportDMA: vkGetMemoryFdPropertiesKHR failed");
        close(impFd);
        return fail();
    }

    VkMemoryRequirements req {};
    vkGetImageMemoryRequirements(vkDev, image, &req);

    const UInt32 memType { dev->findMemoryType(req.memoryTypeBits & fdProps.memoryTypeBits, 0) };
    if (memType == UINT32_MAX)
    {
        RLog(CZError, CZLN, "RVKImage::ImportDMA: no compatible memory type for imported fd");
        close(impFd);
        return fail();
    }

    VkMemoryDedicatedAllocateInfo dedicated {};
    dedicated.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
    dedicated.image = image;

    VkImportMemoryFdInfoKHR importInfo {};
    importInfo.sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR;
    importInfo.pNext = &dedicated;
    importInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;
    importInfo.fd = impFd;

    VkMemoryAllocateInfo ai {};
    ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.pNext = &importInfo;
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = memType;

    if (vkAllocateMemory(vkDev, &ai, nullptr, &memory) != VK_SUCCESS)
    {
        RLog(CZError, CZLN, "RVKImage::ImportDMA: vkAllocateMemory (import) failed");
        close(impFd);
        return fail();
    }

    if (vkBindImageMemory(vkDev, image, memory, 0) != VK_SUCCESS)
    {
        RLog(CZError, CZLN, "RVKImage::ImportDMA: vkBindImageMemory failed");
        return fail();
    }

    VkImageViewCreateInfo vi {};
    vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vi.image = image;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format = vkFmt;
    vi.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    if (vkCreateImageView(vkDev, &vi, nullptr, &view) != VK_SUCCESS)
    {
        RLog(CZError, CZLN, "RVKImage::ImportDMA: vkCreateImageView failed");
        return fail();
    }

    outImage = image;
    outMemory = memory;
    outView = view;
    if (outSize)
        *outSize = req.size;
    return true;
}

bool RVKImage::importDMA(const RDMABufferInfo &info, VkImageUsageFlags usage) noexcept
{
    m_tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT;
    m_usage = usage;
    return ImportDMA(m_dev, info, m_vkFormat, usage, m_image, m_memory, m_view, &m_memorySize);
}

std::optional<RDMABufferInfo> RVKImage::dmaExport() const noexcept
{
    if (m_bo)
        return m_bo->dmaExport();
    return std::nullopt;
}

bool RVKImage::ensureCrossDevice(RVKDevice *device) const noexcept
{
    if (!device || device == m_dev)
        return true; // the allocator uses the primary image

    std::lock_guard<std::mutex> lock { m_mutex };

    const auto it { m_shared.find(device) };
    if (it != m_shared.end())
        return it->second.image != VK_NULL_HANDLE;

    if (!m_exportable)
        return false;

    auto dma { dmaExport() };
    if (!dma)
        return false;

    SharedImage s {};
    const bool ok { ImportDMA(device, *dma, m_vkFormat,
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        s.image, s.memory, s.view) };
    dma->closeFds();

    if (!ok)
        return false;

    m_shared[device] = s;
    return true;
}

VkImage RVKImage::vkImage(RVKDevice *device) const noexcept
{
    if (!device || device == m_dev)
        return m_image;
    std::lock_guard<std::mutex> lock { m_mutex };
    const auto it { m_shared.find(device) };
    return it != m_shared.end() ? it->second.image : VK_NULL_HANDLE;
}

VkImageView RVKImage::vkImageView(RVKDevice *device) const noexcept
{
    if (!device || device == m_dev)
        return m_view;
    std::lock_guard<std::mutex> lock { m_mutex };
    const auto it { m_shared.find(device) };
    return it != m_shared.end() ? it->second.view : VK_NULL_HANDLE;
}

VkImageLayout RVKImage::layout(RVKDevice *device) const noexcept
{
    if (!device || device == m_dev)
        return m_layout;
    std::lock_guard<std::mutex> lock { m_mutex };
    const auto it { m_shared.find(device) };
    return it != m_shared.end() ? it->second.layout : VK_IMAGE_LAYOUT_UNDEFINED;
}

void RVKImage::transitionLayout(VkCommandBuffer cmd, RVKDevice *device, VkImageLayout newLayout,
                                VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage,
                                VkAccessFlags srcAccess, VkAccessFlags dstAccess) const noexcept
{
    if (!device || device == m_dev)
    {
        transitionLayout(cmd, newLayout, srcStage, dstStage, srcAccess, dstAccess);
        return;
    }

    std::lock_guard<std::mutex> lock { m_mutex };
    const auto it { m_shared.find(device) };
    if (it == m_shared.end())
        return;
    auto &s { it->second };

    VkImageMemoryBarrier barrier {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = s.layout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = s.image;
    barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    barrier.srcAccessMask = srcAccess;
    barrier.dstAccessMask = dstAccess;

    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    s.layout = newLayout;
}

bool RVKImage::initNativeStorage() noexcept
{
    const VkDevice dev { m_dev->device() };

    m_tiling = VK_IMAGE_TILING_OPTIMAL;
    m_usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
              VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    VkImageCreateInfo ci {};
    ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ci.imageType = VK_IMAGE_TYPE_2D;
    ci.format = m_vkFormat;
    ci.extent = { (UInt32)size().width(), (UInt32)size().height(), 1 };
    ci.mipLevels = 1;
    ci.arrayLayers = 1;
    ci.samples = VK_SAMPLE_COUNT_1_BIT;
    ci.tiling = m_tiling;
    ci.usage = m_usage;
    ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (vkCreateImage(dev, &ci, nullptr, &m_image) != VK_SUCCESS)
    {
        RLog(CZError, CZLN, "RVKImage: vkCreateImage failed");
        return false;
    }

    VkMemoryRequirements req {};
    vkGetImageMemoryRequirements(dev, m_image, &req);

    const UInt32 memType { m_dev->findMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) };
    if (memType == UINT32_MAX)
    {
        RLog(CZError, CZLN, "RVKImage: no suitable device-local memory type");
        return false;
    }

    VkMemoryAllocateInfo ai {};
    ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = memType;

    if (vkAllocateMemory(dev, &ai, nullptr, &m_memory) != VK_SUCCESS)
    {
        RLog(CZError, CZLN, "RVKImage: vkAllocateMemory failed");
        return false;
    }
    m_memorySize = req.size;

    if (vkBindImageMemory(dev, m_image, m_memory, 0) != VK_SUCCESS)
    {
        RLog(CZError, CZLN, "RVKImage: vkBindImageMemory failed");
        return false;
    }

    VkImageViewCreateInfo vi {};
    vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vi.image = m_image;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format = m_vkFormat;
    vi.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    if (vkCreateImageView(dev, &vi, nullptr, &m_view) != VK_SUCCESS)
    {
        RLog(CZError, CZLN, "RVKImage: vkCreateImageView failed");
        return false;
    }

    return true;
}

void RVKImage::assignReadWriteFormats() noexcept
{
    // Staging transfers require the CPU buffer to already be in the image's native format.
    m_readFormats.emplace(formatInfo().format);
    m_writeFormats.emplace(formatInfo().format);
}

void RVKImage::fillImageInfo(void *out) const noexcept
{
    auto *info { static_cast<GrVkImageInfo*>(out) };
    *info = {};
    info->fImage = m_image;
    info->fAlloc.fMemory = m_memory; // may be VK_NULL_HANDLE for borrowed render targets
    info->fAlloc.fOffset = 0;
    info->fAlloc.fSize = m_memorySize;
    info->fAlloc.fFlags = 0;
    info->fImageTiling = m_tiling;
    info->fImageLayout = m_layout;
    info->fFormat = m_vkFormat;
    info->fImageUsageFlags = m_usage;
    info->fSampleCount = 1;
    info->fLevelCount = 1;
    info->fCurrentQueueFamily = VK_QUEUE_FAMILY_IGNORED;
    info->fProtected = skgpu::Protected::kNo;
    info->fSharingMode = VK_SHARING_MODE_EXCLUSIVE;
}

bool RVKImage::ensureBackendTexture() const noexcept
{
    if (m_hasBackendTexture)
        return true;
    if (m_image == VK_NULL_HANDLE)
        return false;

    GrVkImageInfo info {};
    fillImageInfo(&info);
    m_backendTexture = GrBackendTextures::MakeVk(size().width(), size().height(), info);
    m_hasBackendTexture = m_backendTexture.isValid();
    if (!m_hasBackendTexture)
        RLog(CZError, CZLN, "RVKImage: failed to create Skia backend texture");
    return m_hasBackendTexture;
}

bool RVKImage::ensureBackendRenderTarget() const noexcept
{
    if (m_hasBackendRT)
        return true;
    if (m_image == VK_NULL_HANDLE)
        return false;

    GrVkImageInfo info {};
    fillImageInfo(&info);
    m_backendRT = GrBackendRenderTargets::MakeVk(size().width(), size().height(), info);
    m_hasBackendRT = m_backendRT.isValid();
    if (!m_hasBackendRT)
        RLog(CZError, CZLN, "RVKImage: failed to create Skia backend render target");
    return m_hasBackendRT;
}

RVKImage::ThreadViews &RVKImage::threadViews(RVKDevice *) const noexcept
{
    return m_views[std::this_thread::get_id()];
}

// A Skia render target must use a *renderable* color type. The DRM->Sk mapping yields opaque
// "x" variants (e.g. XBGR8888 -> kRGB_888x) that Skia rejects as surface formats, so derive a
// renderable color type from the VkFormat instead.
static SkColorType SurfaceColorType(VkFormat fmt, SkColorType fallback) noexcept
{
    switch (fmt)
    {
    case VK_FORMAT_R8G8B8A8_UNORM:           return kRGBA_8888_SkColorType;
    case VK_FORMAT_B8G8R8A8_UNORM:           return kBGRA_8888_SkColorType;
    case VK_FORMAT_R5G6B5_UNORM_PACK16:      return kRGB_565_SkColorType;
    case VK_FORMAT_A2B10G10R10_UNORM_PACK32: return kRGBA_1010102_SkColorType;
    case VK_FORMAT_A2R10G10B10_UNORM_PACK32: return kBGRA_1010102_SkColorType;
    case VK_FORMAT_R16G16B16A16_SFLOAT:      return kRGBA_F16_SkColorType;
    case VK_FORMAT_R8_UNORM:                 return kR8_unorm_SkColorType;
    default:                                 return fallback;
    }
}

sk_sp<SkImage> RVKImage::skImage(RDevice *device) const noexcept
{
    RVKDevice *dev { device ? device->asVK() : m_dev };
    if (dev != m_dev) // cross-device sharing handled in a later phase
        return {};

    std::lock_guard<std::mutex> lock { m_mutex };

    if (!ensureBackendTexture())
        return {};

    auto ctx { m_dev->skContext() };
    if (!ctx)
        return {};

    auto &tv { threadViews(dev) };
    if (!tv.image)
    {
        const SkColorType ct { RSKFormat::FromDRM(formatInfo().format) };
        tv.image = SkImages::BorrowTextureFrom(ctx.get(), m_backendTexture,
                                               kTopLeft_GrSurfaceOrigin, ct, alphaType(), nullptr);
    }
    return tv.image;
}

sk_sp<SkSurface> RVKImage::skSurface(RDevice *device) const noexcept
{
    RVKDevice *dev { device ? device->asVK() : m_dev };
    if (dev != m_dev)
        return {};

    std::lock_guard<std::mutex> lock { m_mutex };

    if (!ensureBackendRenderTarget())
        return {};

    auto ctx { m_dev->skContext() };
    if (!ctx)
        return {};

    auto &tv { threadViews(dev) };
    if (!tv.surface)
    {
        const SkColorType ct { SurfaceColorType(m_vkFormat, RSKFormat::FromDRM(formatInfo().format)) };
        tv.surface = SkSurfaces::WrapBackendRenderTarget(ctx.get(), m_backendRT,
                                                         kTopLeft_GrSurfaceOrigin, ct, nullptr, nullptr);
    }
    return tv.surface;
}

std::shared_ptr<RGBMBo> RVKImage::gbmBo(RDevice *device) const noexcept
{
    RVKDevice *dev { device ? device->asVK() : m_dev };
    if (dev != m_dev)
        return {};
    return m_bo;
}

std::shared_ptr<RDRMFramebuffer> RVKImage::drmFb(RDevice *device) const noexcept
{
    RVKDevice *dev { device ? device->asVK() : m_dev };
    if (dev != m_dev || !m_bo)
        return {};

    std::lock_guard<std::mutex> lock { m_mutex };
    if (!m_drmFb)
        m_drmFb = RDRMFramebuffer::MakeFromGBMBo(m_bo);
    return m_drmFb;
}

CZBitset<RImageCap> RVKImage::checkDeviceCaps(CZBitset<RImageCap> caps, RDevice *device) const noexcept
{
    RVKDevice *dev { device ? device->asVK() : m_dev };
    CZBitset<RImageCap> ret { 0 };

    if (dev != m_dev)
    {
        // On a non-allocator device only sampling (Src) is offered, via cross-device dma import.
        if (caps.has(RImageCap_Src) && ensureCrossDevice(dev))
            ret.add(RImageCap_Src);
        return ret;
    }

    if (caps.has(RImageCap_Src) && m_view != VK_NULL_HANDLE)
        ret.add(RImageCap_Src);
    if (caps.has(RImageCap_Dst) && (m_usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT))
        ret.add(RImageCap_Dst);
    if (caps.has(RImageCap_SkImage) && skImage(dev))
        ret.add(RImageCap_SkImage);
    if (caps.has(RImageCap_SkSurface) && skSurface(dev))
        ret.add(RImageCap_SkSurface);
    if (caps.has(RImageCap_GBMBo) && gbmBo(dev))
        ret.add(RImageCap_GBMBo);
    if (caps.has(RImageCap_DRMFb) && drmFb(dev))
        ret.add(RImageCap_DRMFb);

    return ret;
}

void RVKImage::transitionLayout(VkCommandBuffer cmd, VkImageLayout newLayout,
                                VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage,
                                VkAccessFlags srcAccess, VkAccessFlags dstAccess) const noexcept
{
    VkImageMemoryBarrier barrier {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = m_layout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_image;
    barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    barrier.srcAccessMask = srcAccess;
    barrier.dstAccessMask = dstAccess;

    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    m_layout = newLayout;

    if (m_hasBackendTexture)
        GrBackendTextures::SetVkImageLayout(&m_backendTexture, newLayout);
    if (m_hasBackendRT)
        GrBackendRenderTargets::SetVkImageLayout(&m_backendRT, newLayout);
}

void RVKImage::setTrackedLayout(VkImageLayout layout) const noexcept
{
    std::lock_guard<std::mutex> lock { m_mutex };
    m_layout = layout;
    if (m_hasBackendTexture)
        GrBackendTextures::SetVkImageLayout(&m_backendTexture, layout);
    if (m_hasBackendRT)
        GrBackendRenderTargets::SetVkImageLayout(&m_backendRT, layout);
}

void RVKImage::syncLayoutFromSkia() const noexcept
{
    std::lock_guard<std::mutex> lock { m_mutex };

    GrVkImageInfo info {};
    // The render target is the object Skia renders into, so it holds the authoritative layout.
    if (m_hasBackendRT && GrBackendRenderTargets::GetVkImageInfo(m_backendRT, &info))
        m_layout = info.fImageLayout;
    else if (m_hasBackendTexture && GrBackendTextures::GetVkImageInfo(m_backendTexture, &info))
        m_layout = info.fImageLayout;
}

bool RVKImage::writePixels(const RPixelBufferRegion &region) noexcept
{
    if (region.format != formatInfo().format)
    {
        RLog(CZError, CZLN, "RVKImage::writePixels: format mismatch");
        return false;
    }
    if (region.region.isEmpty())
        return true;

    const VkDevice dev { m_dev->device() };
    const UInt32 bpb { formatInfo().bytesPerBlock };

    // Pack the requested rects tightly into a host-visible staging buffer.
    std::vector<VkBufferImageCopy> copies;
    VkDeviceSize total { 0 };
    for (SkRegion::Iterator it(region.region); !it.done(); it.next())
    {
        const SkIRect &r { it.rect() };
        total += (VkDeviceSize)r.width() * r.height() * bpb;
    }
    if (total == 0)
        return true;

    VkBufferCreateInfo bi {};
    bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bi.size = total;
    bi.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer staging { VK_NULL_HANDLE };
    VkDeviceMemory stagingMem { VK_NULL_HANDLE };
    bool ok { false };
    void *mapped { nullptr };

    if (vkCreateBuffer(dev, &bi, nullptr, &staging) != VK_SUCCESS)
        return false;

    {
        VkMemoryRequirements req {};
        vkGetBufferMemoryRequirements(dev, staging, &req);
        const UInt32 memType { m_dev->findMemoryType(req.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) };
        if (memType == UINT32_MAX)
            goto cleanup;

        VkMemoryAllocateInfo ai {};
        ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        ai.allocationSize = req.size;
        ai.memoryTypeIndex = memType;
        if (vkAllocateMemory(dev, &ai, nullptr, &stagingMem) != VK_SUCCESS)
            goto cleanup;
        if (vkBindBufferMemory(dev, staging, stagingMem, 0) != VK_SUCCESS)
            goto cleanup;
        if (vkMapMemory(dev, stagingMem, 0, total, 0, &mapped) != VK_SUCCESS)
            goto cleanup;
    }

    {
        auto *base { static_cast<UInt8*>(mapped) };
        VkDeviceSize off { 0 };
        for (SkRegion::Iterator it(region.region); !it.done(); it.next())
        {
            const SkIRect &r { it.rect() };
            const int srcX { r.left() + region.offset.x() };
            const int srcY { r.top() + region.offset.y() };
            const UInt32 rowBytes { (UInt32)r.width() * bpb };

            for (int row = 0; row < r.height(); row++)
            {
                const UInt8 *src { region.pixels + (VkDeviceSize)(srcY + row) * region.stride + (VkDeviceSize)srcX * bpb };
                std::memcpy(base + off + (VkDeviceSize)row * rowBytes, src, rowBytes);
            }

            VkBufferImageCopy c {};
            c.bufferOffset = off;
            c.bufferRowLength = r.width();
            c.bufferImageHeight = r.height();
            c.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
            c.imageOffset = { r.left(), r.top(), 0 };
            c.imageExtent = { (UInt32)r.width(), (UInt32)r.height(), 1 };
            copies.push_back(c);

            off += (VkDeviceSize)r.width() * r.height() * bpb;
        }
        vkUnmapMemory(dev, stagingMem);
    }

    ok = m_dev->immediateSubmit([&](VkCommandBuffer cmd)
    {
        transitionLayout(cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, VK_ACCESS_TRANSFER_WRITE_BIT);

        vkCmdCopyBufferToImage(cmd, staging, m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               copies.size(), copies.data());

        transitionLayout(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
    });

    if (ok)
        m_writeSerial++;

cleanup:
    if (stagingMem != VK_NULL_HANDLE)
        vkFreeMemory(dev, stagingMem, nullptr);
    if (staging != VK_NULL_HANDLE)
        vkDestroyBuffer(dev, staging, nullptr);
    return ok;
}

bool RVKImage::readPixels(const RPixelBufferRegion &region) noexcept
{
    if (region.format != formatInfo().format)
    {
        RLog(CZError, CZLN, "RVKImage::readPixels: format mismatch");
        return false;
    }
    if (region.region.isEmpty())
        return true;

    const VkDevice dev { m_dev->device() };
    const UInt32 bpb { formatInfo().bytesPerBlock };

    std::vector<VkBufferImageCopy> copies;
    std::vector<SkIRect> rects;
    std::vector<VkDeviceSize> offsets;
    VkDeviceSize total { 0 };
    for (SkRegion::Iterator it(region.region); !it.done(); it.next())
    {
        const SkIRect &r { it.rect() };
        const int srcX { r.x() + region.offset.x() };
        const int srcY { r.y() + region.offset.y() };
        if (srcX < 0 || srcY < 0 || r.width() <= 0 || r.height() <= 0)
            continue;

        VkBufferImageCopy c {};
        c.bufferOffset = total;
        c.bufferRowLength = r.width();
        c.bufferImageHeight = r.height();
        c.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
        c.imageOffset = { srcX, srcY, 0 };
        c.imageExtent = { (UInt32)r.width(), (UInt32)r.height(), 1 };
        copies.push_back(c);
        rects.push_back(r);
        offsets.push_back(total);

        total += (VkDeviceSize)r.width() * r.height() * bpb;
    }
    if (total == 0)
        return true;

    VkBufferCreateInfo bi {};
    bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bi.size = total;
    bi.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer staging { VK_NULL_HANDLE };
    VkDeviceMemory stagingMem { VK_NULL_HANDLE };
    bool ok { false };
    void *mapped { nullptr };

    if (vkCreateBuffer(dev, &bi, nullptr, &staging) != VK_SUCCESS)
        return false;

    {
        VkMemoryRequirements req {};
        vkGetBufferMemoryRequirements(dev, staging, &req);
        const UInt32 memType { m_dev->findMemoryType(req.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) };
        if (memType == UINT32_MAX)
            goto cleanup;

        VkMemoryAllocateInfo ai {};
        ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        ai.allocationSize = req.size;
        ai.memoryTypeIndex = memType;
        if (vkAllocateMemory(dev, &ai, nullptr, &stagingMem) != VK_SUCCESS)
            goto cleanup;
        if (vkBindBufferMemory(dev, staging, stagingMem, 0) != VK_SUCCESS)
            goto cleanup;
    }

    ok = m_dev->immediateSubmit([&](VkCommandBuffer cmd)
    {
        const VkImageLayout prev { m_layout };
        transitionLayout(cmd, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, VK_ACCESS_TRANSFER_READ_BIT);

        vkCmdCopyImageToBuffer(cmd, m_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               staging, copies.size(), copies.data());

        const VkImageLayout rest { prev == VK_IMAGE_LAYOUT_UNDEFINED ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : prev };
        transitionLayout(cmd, rest,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT);
    });

    if (!ok)
        goto cleanup;

    if (vkMapMemory(dev, stagingMem, 0, total, 0, &mapped) != VK_SUCCESS)
    {
        ok = false;
        goto cleanup;
    }

    {
        auto *base { static_cast<const UInt8*>(mapped) };
        for (size_t i = 0; i < rects.size(); i++)
        {
            const SkIRect &r { rects[i] };
            const UInt32 rowBytes { (UInt32)r.width() * bpb };
            for (int row = 0; row < r.height(); row++)
            {
                const UInt8 *src { base + offsets[i] + (VkDeviceSize)row * rowBytes };
                UInt8 *dst { region.pixels + (VkDeviceSize)(r.y() + row) * region.stride + (VkDeviceSize)r.x() * bpb };
                std::memcpy(dst, src, rowBytes);
            }
        }
        vkUnmapMemory(dev, stagingMem);
    }

cleanup:
    if (stagingMem != VK_NULL_HANDLE)
        vkFreeMemory(dev, stagingMem, nullptr);
    if (staging != VK_NULL_HANDLE)
        vkDestroyBuffer(dev, staging, nullptr);
    return ok;
}
