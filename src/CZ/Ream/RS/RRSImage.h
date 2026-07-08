#ifndef CZ_RRSIMAGE_H
#define CZ_RRSIMAGE_H

#include <CZ/Ream/RImage.h>
#include <CZ/Core/CZSharedMemory.h>
#include <CZ/Core/CZBitset.h>
#include <EGL/egl.h>

/**
 * @brief Raster (software) implementation of RImage.
 *
 * RRSImage backs its pixels with host-visible shared memory (CZSharedMemory) using a linear layout,
 * and wraps that memory in both an SkImage (for sampling) and an SkSurface (for rendering). It can
 * be obtained from an RImage via asRS().
 *
 * Because it is a CPU buffer, it never exposes GBM buffer objects or DRM framebuffers. The shared
 * memory can be shared with a Wayland compositor (see RRSSwapchainWL) as a @c wl_shm buffer.
 */
class CZ::RRSImage : public RImage
{
public:
    /**
     * @brief Creates a Raster image.
     *
     * Allocates shared memory sized for @p size and @p format (which must be a linear-modifier
     * format supported by the allocator device) and wraps it in an SkImage and SkSurface.
     *
     * @param size        Image size in pixels. Must not be empty.
     * @param format      DRM format and modifiers. Must resolve to a supported Skia color type and
     *                    allow the linear (or invalid) modifier.
     * @param constraints Optional constraints. Note that the DRMFb and GBMBo capabilities are not
     *                    supported and requesting them fails.
     *
     * @return A valid RRSImage on success, nullptr on failure.
     */
    [[nodiscard]] static std::shared_ptr<RRSImage> Make(SkISize size, const RDRMFormat &format, const RImageConstraints *constraints = nullptr) noexcept;

    /**
     * @brief Always returns an empty pointer.
     *
     * Raster images are CPU buffers and have no GBM backing.
     */
    std::shared_ptr<RGBMBo> gbmBo(RDevice *device = nullptr) const noexcept override
    {
        CZ_UNUSED(device)
        return {};
    }

    /**
     * @brief Always returns an empty pointer.
     *
     * Raster images cannot be used as DRM framebuffers.
     */
    std::shared_ptr<RDRMFramebuffer> drmFb(RDevice *device = nullptr) const noexcept override
    {
        CZ_UNUSED(device)
        return {};
    }

    /**
     * @brief Returns the SkImage wrapping the shared-memory pixels.
     *
     * The @p device argument is ignored; the same object is returned regardless of device.
     */
    sk_sp<SkImage> skImage(RDevice *device = nullptr) const noexcept override
    {
        CZ_UNUSED(device)
        return m_skImage;
    }

    /**
     * @brief Returns the SkSurface wrapping the shared-memory pixels.
     *
     * The @p device argument is ignored; the same object is returned regardless of device.
     */
    sk_sp<SkSurface> skSurface(RDevice *device = nullptr) const noexcept override
    {
        CZ_UNUSED(device)
        return m_skSurface;
    }

    /**
     * @brief Reports which of the requested capabilities are supported.
     *
     * Implements the RImage contract for the Raster backend: the DRMFb and GBMBo capabilities are
     * always stripped from @p caps, all others are reported as supported.
     */
    CZBitset<RImageCap> checkDeviceCaps(CZBitset<RImageCap> caps, RDevice *device = nullptr) const noexcept override;

    /**
     * @brief Uploads pixels into the image via a CPU memcpy.
     *
     * The region's format must match the image format. Implements the RImage writePixels() contract.
     */
    bool writePixels(const RPixelBufferRegion &region) noexcept override;

    /**
     * @brief Downloads pixels from the image via a CPU memcpy.
     *
     * The region's format must match the image format. Implements the RImage readPixels() contract.
     */
    bool readPixels(const RPixelBufferRegion &region) noexcept override;

    /**
     * @brief Returns the allocator device downcast to the Raster backend type.
     */
    RRSDevice *allocator() const noexcept { return (RRSDevice*)m_allocator; }

    /**
     * @brief Returns the row stride of the pixel buffer, in bytes.
     */
    size_t stride() const noexcept { return m_stride; }

    /**
     * @brief Returns the shared memory backing the image pixels.
     */
    std::shared_ptr<CZSharedMemory> shm() const noexcept { return m_shm; };

    /**
     * @brief Resizes the image in place, reallocating the backing SkImage/SkSurface.
     *
     * The underlying shared memory is grown only when the new size needs more bytes than currently
     * allocated; it is never shrunk. Used by RRSSwapchainWL to recycle buffers.
     *
     * @param size The new image size in pixels. Must not be empty.
     *
     * @return -1 on error, 0 if the shared memory was not truncated (reused as-is),
     *         1 if the shared memory was truncated (resized).
     */
    int resize(SkISize size) noexcept;
private:
    RRSImage(std::shared_ptr<RCore> core, std::shared_ptr<CZSharedMemory> shm, sk_sp<SkImage> skImage, sk_sp<SkSurface> skSurface,
             RDevice *device, SkISize size, size_t stride, const RFormatInfo *formatInfo, SkAlphaType alphaType, RModifier modifier) noexcept;
    size_t m_stride;
    std::shared_ptr<CZSharedMemory> m_shm;
    sk_sp<SkImage> m_skImage;
    sk_sp<SkSurface> m_skSurface;
};
#endif // CZ_RRSIMAGE_H
