#ifndef CZ_RVKIMAGE_H
#define CZ_RVKIMAGE_H

#include <CZ/skia/gpu/ganesh/GrBackendSurface.h>
#include <CZ/Ream/RImage.h>
#include <CZ/Core/CZOwn.h>
#include <vulkan/vulkan.h>
#include <unordered_map>
#include <optional>
#include <thread>
#include <mutex>

/**
 * @brief Vulkan implementation of RImage, obtained via RImage::asVK().
 *
 * Backs the image with either native (VK_IMAGE_TILING_OPTIMAL, device-local) storage or
 * GBM/dma-buf storage imported via VK_EXT_image_drm_format_modifier + external memory (for scanout
 * and cross-device sharing). Explicit DRM format modifiers are used; multi-plane buffers are only
 * supported when all planes reside in a single dma-buf memory object.
 *
 * The tracked VkImageLayout is kept in sync with Skia's backend texture/render target so manual
 * barriers and Skia rendering can interoperate. For cross-device sampling (SRM Prime) the image is
 * lazily re-imported on the sampling device, keyed per device.
 */
class CZ::RVKImage final : public RImage
{
public:
    /**
     * @brief Creates a Vulkan-backed image of the given size and DRM format.
     *
     * Uses GBM/dma-buf storage when scanout (DRMFb/GBMBo) or cross-device caps are requested and
     * the required DMA extensions are available; otherwise native optimal storage. Returns nullptr
     * if the format is unsupported or the constraints cannot be satisfied.
     */
    [[nodiscard]] static std::shared_ptr<RVKImage> Make(SkISize size, const RDRMFormat &format, const RImageConstraints *constraints = nullptr) noexcept;

    /**
     * @brief Imports an existing dma-buf as a Vulkan-backed image.
     *
     * Requires VK_EXT_image_drm_format_modifier and VK_EXT_external_memory_dma_buf. When
     * @p ownership is CZOwn::Own the source fds are closed before returning (on success or failure).
     */
    [[nodiscard]] static std::shared_ptr<RVKImage> FromDMA(const RDMABufferInfo &info, CZOwn ownership, const RImageConstraints *constraints = nullptr) noexcept;

    // Wraps an externally-owned VkImage (e.g. a VkSwapchainKHR image). The image and its memory
    // are NOT destroyed by this object; only the created view is owned.
    [[nodiscard]] static std::shared_ptr<RVKImage> WrapVkImage(RVKDevice *device, VkImage image, VkFormat format,
                                                               SkISize size, VkImageUsageFlags usage, bool alpha) noexcept;

    // Sets the tracked layout directly (used by the swapchain after present -> PRESENT_SRC).
    void setLayout(VkImageLayout layout) const noexcept { m_layout = layout; }

    // Sets the tracked layout and syncs Skia's backend layout, without recording a barrier
    // (the caller guarantees the image is already in this layout, e.g. a render pass finalLayout).
    void setTrackedLayout(VkImageLayout layout) const noexcept;

    // RImage overrides
    std::shared_ptr<RGBMBo> gbmBo(RDevice *device = nullptr) const noexcept override;
    std::shared_ptr<RDRMFramebuffer> drmFb(RDevice *device = nullptr) const noexcept override;
    sk_sp<SkImage> skImage(RDevice *device = nullptr) const noexcept override;
    sk_sp<SkSurface> skSurface(RDevice *device = nullptr) const noexcept override;
    CZBitset<RImageCap> checkDeviceCaps(CZBitset<RImageCap> caps, RDevice *device = nullptr) const noexcept override;
    bool writePixels(const RPixelBufferRegion &region) noexcept override;
    bool readPixels(const RPixelBufferRegion &region) noexcept override;

    // VK-specific accessors (used by RVKPass/RVKPainter). The device-less variants refer to the
    // allocator device (m_dev); the device-taking variants return the per-device shared image
    // (cross-device sampling), lazily importing it via dma-buf.
    /** @brief The device that allocated this image (owner of the primary VkImage). */
    RVKDevice *allocatorVK() const noexcept { return m_dev; }

    /** @brief The primary VkImage (on the allocator device). */
    VkImage vkImage() const noexcept { return m_image; }

    /** @brief The primary VkImageView (on the allocator device). */
    VkImageView vkImageView() const noexcept { return m_view; }

    /** @brief The VkImage for @p device: the primary image on the allocator, else the per-device
     *         cross-device import (VK_NULL_HANDLE if not imported). */
    VkImage vkImage(RVKDevice *device) const noexcept;

    /** @brief The VkImageView for @p device (see vkImage(RVKDevice*)). */
    VkImageView vkImageView(RVKDevice *device) const noexcept;

    /** @brief The image's VkFormat. */
    VkFormat vkFormat() const noexcept { return m_vkFormat; }

    /** @brief The image's VkImageUsageFlags. */
    VkImageUsageFlags vkUsage() const noexcept { return m_usage; }

    /** @brief The image's VkImageTiling (OPTIMAL for native storage, DRM_FORMAT_MODIFIER for dma-buf). */
    VkImageTiling vkTiling() const noexcept { return m_tiling; }

    /**
     * @brief dma-buf export of this image (single plane), for cross-device sharing / scanout.
     *
     * Only available for dma-shareable storage (GBM-backed). The caller must close the returned fds.
     */
    std::optional<RDMABufferInfo> dmaExport() const noexcept;

    /**
     * @brief Ensures this image is sampleable on @p device by importing its dma-buf there.
     *
     * @return true if the image is (now) usable as a source on @p device, false if it is not
     *         dma-shareable or the import failed. Enables SRM's multi-GPU Prime path.
     */
    bool ensureCrossDevice(RVKDevice *device) const noexcept;

    /**
     * @brief Records a layout transition barrier on the given command buffer.
     *
     * Updates the tracked layout and, if the image is wrapped by Skia, informs Skia
     * of the new layout so both stay consistent. The device-taking variant transitions the
     * per-device shared image instead of the allocator's.
     */
    void transitionLayout(VkCommandBuffer cmd, VkImageLayout newLayout,
                          VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage,
                          VkAccessFlags srcAccess, VkAccessFlags dstAccess) const noexcept;
    void transitionLayout(VkCommandBuffer cmd, RVKDevice *device, VkImageLayout newLayout,
                          VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage,
                          VkAccessFlags srcAccess, VkAccessFlags dstAccess) const noexcept;

    /** @brief The tracked VkImageLayout of the primary image. */
    VkImageLayout layout() const noexcept { return m_layout; }

    /** @brief The tracked VkImageLayout for @p device (primary image on the allocator, else the
     *         per-device cross-device import). */
    VkImageLayout layout(RVKDevice *device) const noexcept;

    /**
     * @brief Reconciles the tracked layout with Skia's after Skia rendered into this image.
     *
     * Skia mutates the wrapped image's layout internally; call this after flushing a Skia pass
     * so subsequent manual barriers (pixel transfers, scanout) use the correct old layout.
     */
    void syncLayoutFromSkia() const noexcept;

    ~RVKImage() noexcept;
private:
    RVKImage(std::shared_ptr<RCore> core, RVKDevice *device, SkISize size,
             const RFormatInfo *formatInfo, SkAlphaType alphaType, RModifier modifier) noexcept;

    bool initNativeStorage() noexcept;

    // GBM-backed scanout storage: allocate an RGBMBo and alias a VkImage over its dma-buf.
    bool initGBMStorage(const RDRMFormat &format) noexcept;

    // Imports a dma-buf (single plane) into m_image via VK_EXT_image_drm_format_modifier.
    bool importDMA(const RDMABufferInfo &info, VkImageUsageFlags usage) noexcept;

    // Imports a single-plane dma-buf as a VkImage(+view) on an arbitrary device.
    static bool ImportDMA(RVKDevice *device, const RDMABufferInfo &info, VkFormat vkFmt, VkImageUsageFlags usage,
                          VkImage &outImage, VkDeviceMemory &outMemory, VkImageView &outView, VkDeviceSize *outSize = nullptr) noexcept;

    bool ensureBackendTexture() const noexcept;
    bool ensureBackendRenderTarget() const noexcept;
    void fillImageInfo(void *grVkImageInfo) const noexcept;
    void assignReadWriteFormats() noexcept;

    struct ThreadViews
    {
        sk_sp<SkImage> image;
        sk_sp<SkSurface> surface;
    };
    ThreadViews &threadViews(RVKDevice *device) const noexcept;

    RVKDevice *m_dev { nullptr };

    VkImage m_image { VK_NULL_HANDLE };
    VkDeviceMemory m_memory { VK_NULL_HANDLE };
    VkImageView m_view { VK_NULL_HANDLE };
    VkDeviceSize m_memorySize { 0 };
    VkImageTiling m_tiling { VK_IMAGE_TILING_OPTIMAL };
    VkFormat m_vkFormat { VK_FORMAT_UNDEFINED };
    VkImageUsageFlags m_usage { 0 };
    bool m_ownsImage { true };
    bool m_ownsMemory { true };

    // Canonical layout of the image; mirrored into the Skia backend texture when present.
    mutable VkImageLayout m_layout { VK_IMAGE_LAYOUT_UNDEFINED };

    // Descriptors shared by all per-thread Skia views (hold shared mutable layout state).
    // Texture backs skImage() (sampling); render target backs skSurface() (rendering, and
    // supports borrowed images with no owned VkDeviceMemory, e.g. swapchain images).
    mutable GrBackendTexture m_backendTexture;
    mutable bool m_hasBackendTexture { false };
    mutable GrBackendRenderTarget m_backendRT;
    mutable bool m_hasBackendRT { false };

    mutable std::unordered_map<std::thread::id, ThreadViews> m_views;
    mutable std::mutex m_mutex;

    // Scanout / DMA interop (GBM storage or imported buffers). m_bo lives in the base class.
    mutable std::shared_ptr<RDRMFramebuffer> m_drmFb;

    // Whether this image can be dma-exported (shared across devices) — true for GBM storage.
    bool m_exportable { false };

    // Per-device imported copies for cross-device sampling (Prime), keyed by the sampling device.
    struct SharedImage
    {
        VkImage image { VK_NULL_HANDLE };
        VkDeviceMemory memory { VK_NULL_HANDLE };
        VkImageView view { VK_NULL_HANDLE };
        mutable VkImageLayout layout { VK_IMAGE_LAYOUT_UNDEFINED };
    };
    mutable std::unordered_map<RVKDevice*, SharedImage> m_shared;
};

#endif // CZ_RVKIMAGE_H
