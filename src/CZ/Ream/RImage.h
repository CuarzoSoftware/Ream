#ifndef RIMAGE_H
#define RIMAGE_H

#include <CZ/Ream/RObject.h>
#include <CZ/Ream/RDMABufferInfo.h>
#include <CZ/Ream/DRM/RDRMFormat.h>
#include <CZ/skia/core/SkSize.h>
#include <CZ/skia/core/SkRegion.h>
#include <CZ/skia/core/SkAlphaType.h>
#include <CZ/skia/core/SkImage.h>
#include <CZ/skia/core/SkSurface.h>
#include <CZ/Core/CZWeak.h>
#include <CZ/Core/CZOwn.h>
#include <filesystem>
#include <memory>
#include <unordered_set>

namespace CZ
{
    /**
     * @brief Checks whether an SkAlphaType is a valid, known value.
     *
     * @param alphaType The alpha type to test.
     * @return true if @p alphaType is a valid enumerator (not kUnknown_SkAlphaType), false otherwise.
     */
    static inline constexpr bool SkAlphaTypeIsValid(SkAlphaType alphaType) noexcept
    {
        return alphaType > kUnknown_SkAlphaType && alphaType <= kLastEnum_SkAlphaType;
    }

    /**
     * @brief Describes a pixel buffer.
     *
     * Contains metadata and a pointer to the raw pixel data for an image buffer.
     */
    struct RPixelBufferInfo
    {
        /**
         * @brief Size of the buffer in pixels.
         */
        SkISize size;

        /**
         * @brief Number of bytes per row in the buffer.
         */
        UInt32 stride;

        /**
         * @brief Format of the pixel buffer.
         */
        RFormat format;

        /**
         * @brief Pointer to the top-left corner of the pixel data.
         */
        UInt8 *pixels;

        /**
         * @brief Alpha type of the pixel data.
         *
         * Defaults to kUnknown_SkAlphaType.
         */
        SkAlphaType alphaType { kUnknown_SkAlphaType };
    };

    /**
     * @brief Describes a region to be copied from/to a pixel buffer.
     */
    struct RPixelBufferRegion
    {
        /**
         * @brief Offset in pixels applied to each source rect before copying.
         *
         * @note The offset is only applied to the source.
         *
         * For example:
         * - To copy src(10, 10, 100, 100) to dest(0, 0, 100, 100),
         *   set offset = (10, 10) and region = (0, 0, 100, 100).
         *
         * - To copy src(0, 0, 100, 100) to dest(10, 10, 100, 100),
         *   set offset = (-10, -10) and region = (10, 10, 100, 100).
         */
        SkIPoint offset;

        /**
         * @brief Stride in bytes of the pixel buffer.
         *
         * - If used in RImage::writePixels() it represents the stride of the source buffer.
         * - If used in RImage::readPixels() it represents the stride of the destination buffer.
         */
        UInt32 stride;

        /**
         * @brief Pointer to the top-left corner of the pixel buffer.
         *
         * - If used in RImage::writePixels() it represents the source buffer.
         * - If used in RImage::readPixels() it represents the destination buffer.
         */
        UInt8 *pixels;

        /**
         * @brief Set of rectangles to copy, relative to the top-left corner of the source buffer.
         */
        SkRegion region;

        /**
         * @brief The DRM format of the 'pixels' parameter.
         */
        RFormat format;

        /**
         * @brief Computes the address of a pixel within a buffer.
         *
         * @param origin        Pointer to the top-left corner of the buffer.
         * @param offset        Pixel offset from the origin.
         * @param bytesPerPixel Number of bytes per pixel.
         * @param stride        Number of bytes per row.
         * @return Pointer to the pixel at @p offset.
         */
        static constexpr UInt8 *AddressAt(UInt8 *origin, SkIPoint offset, UInt32 bytesPerPixel, UInt32 stride) noexcept
        {
            return origin + (offset.y() * stride) + (offset.x() * bytesPerPixel);
        }
    };

    /**
     * @brief Backing storage type for an RImage.
     */
    enum class RStorageType
    {
        Auto,   ///< Let the backend choose the most suitable storage.
        Native, ///< API-optimal native storage.
        GBM     ///< GBM/DMA-buf storage, suitable for KMS scanout and cross-device sharing.
    };

    /// Device-specific caps
    enum RImageCap : UInt32
    {
        /// Can be used as a src image by an RPass (RPass::drawImage())
        RImageCap_Src       = 1u << 0,

        /// Can be used as a render destination in an RPass (accepted by RSurface::WrapImage)
        RImageCap_Dst       = 1u << 1,

        /// Can be used as a src image by an RSKPass (has an SkImage)
        RImageCap_SkImage   = 1u << 2,

        /// Can be used as a render destination in an RSKPass (has an SkSurface)
        RImageCap_SkSurface = 1u << 3,

        /// Can provide a DRM framebuffer (drmFb()), usable for KMS scanout.
        RImageCap_DRMFb     = 1u << 4,

        /// Backed by a GBM buffer object (gbmBo()).
        RImageCap_GBMBo     = 1u << 5,

        /// All of the above caps combined.
        RImageCap_All       = 0x3F
    };

    /**
     * @brief Constraints applied when allocating an RImage.
     *
     * Used by the RImage factory functions to require specific capabilities and formats.
     */
    struct RImageConstraints
    {
        /// Required caps for each device. Leave empty to disable constraints.
        std::unordered_map<RDevice*, CZBitset<RImageCap>> caps;

        /// Required read formats (the image must support at least one). Leave empty to disable constraints.
        std::unordered_set<RFormat> readFormats;

        /// Required write formats (the image must support at least one). Leave empty to disable constraints.
        std::unordered_set<RFormat> writeFormats;

        /// The allocator device; if nullptr, RCore::mainDevice() is used.
        RDevice *allocator { nullptr };
    };
}

/**
 * @brief A GPU-accessible image buffer.
 *
 * RImage is the fundamental buffer type: a 2D image that can be used as a sampling source, a
 * render target (via RSurface), or shared with other devices and processes.
 *
 * Images may be backed by different storage depending on the requested capabilities: native
 * (API-optimal) storage, GBM/DMA-buf storage suitable for KMS scanout and cross-device sharing, or
 * dumb buffers. An image can be created with RImage::Make(), imported from a DMA-buf with
 * RImage::FromDMA(), or wrapped around an existing backend handle.
 *
 * Pixel data can be uploaded/downloaded with writePixels()/readPixels(), and access is coordinated
 * across threads and devices through per-image read/write RSync fences.
 */
class CZ::RImage : public RObject
{
public:
    /**
     * @brief Allocates a new empty image.
     *
     * @param size        Image size in pixels.
     * @param format      Requested DRM format (and modifiers).
     * @param constraints Optional allocation constraints (caps, formats, allocator device).
     * @return A shared pointer to the new image, or nullptr on failure.
     */
    [[nodiscard]] static std::shared_ptr<RImage> Make(SkISize size, const RDRMFormat &format, const RImageConstraints *constraints = nullptr) noexcept;

    /**
     * @brief Allocates a new image and uploads pixel data into it.
     *
     * The source format (@c info.format) is automatically added to the required write formats.
     *
     * @param info        Description of the source pixel buffer.
     * @param format      Requested DRM format (and modifiers) for the new image.
     * @param constraints Optional allocation constraints.
     * @return A shared pointer to the new image, or nullptr on failure.
     */
    [[nodiscard]] static std::shared_ptr<RImage> MakeFromPixels(const RPixelBufferInfo &info, const RDRMFormat &format, const RImageConstraints *constraints = nullptr) noexcept;

    /**
     * @brief Loads an image from a file.
     *
     * Supports common raster formats and SVG. The decoded content is scaled/converted to
     * @p format and, if given, @p size.
     *
     * @param size        Desired output size in pixels; {0, 0} keeps the source dimensions.
     * @param format      Requested DRM format for the new image.
     * @param path        Path to the image file.
     * @param constraints Optional allocation constraints.
     * @return A shared pointer to the new image, or nullptr on failure.
     */
    [[nodiscard]] static std::shared_ptr<RImage> LoadFile(const std::filesystem::path &path, const RDRMFormat &format, SkISize size = {0, 0}, const RImageConstraints *constraints = nullptr) noexcept;

    /**
     * @brief Imports an image from a DMA-buf.
     *
     * @param info        Description of the DMA buffer (fds, offsets, strides, modifier).
     * @param ownership   Whether the image takes ownership of the buffer's file descriptors.
     *                    When CZOwn::Own, the fds are closed on failure.
     * @param constraints Optional allocation constraints.
     * @return A shared pointer to the imported image, or nullptr on failure.
     */
    [[nodiscard]] static std::shared_ptr<RImage> FromDMA(const RDMABufferInfo &info, CZOwn ownership, const RImageConstraints *constraints = nullptr) noexcept;

    /**
     * @brief Returns the GBM buffer object backing this image, if any.
     *
     * @param device Device to query. If nullptr, the main device is used.
     * @return The GBM buffer object, or nullptr if unavailable.
     */
    virtual std::shared_ptr<RGBMBo> gbmBo(RDevice *device = nullptr) const noexcept = 0;

    /**
     * @brief Returns a DRM framebuffer for this image, if supported.
     *
     * @param device Device to query. If nullptr, the main device is used.
     * @return The DRM framebuffer, or nullptr if unavailable.
     */
    virtual std::shared_ptr<RDRMFramebuffer> drmFb(RDevice *device = nullptr) const noexcept = 0;

    /**
     * @brief Returns an SkImage view of this image, if supported (RImageCap_SkImage).
     *
     * @param device Device to query. If nullptr, the main device is used.
     * @return The SkImage, or nullptr if unavailable.
     */
    virtual sk_sp<SkImage> skImage(RDevice *device = nullptr) const noexcept = 0;

    /**
     * @brief Returns an SkSurface targeting this image, if supported (RImageCap_SkSurface).
     *
     * @param device Device to query. If nullptr, the main device is used.
     * @return The SkSurface, or nullptr if unavailable.
     */
    virtual sk_sp<SkSurface> skSurface(RDevice *device = nullptr) const noexcept = 0;

    // If caps == ret then all tested caps are supported
    /**
     * @brief Checks which of the given capabilities are supported on a device.
     *
     * @param caps   The capabilities to test.
     * @param device Device to query. If nullptr, the main device is used.
     * @return The subset of @p caps that is supported; equals @p caps when all are supported.
     */
    virtual CZBitset<RImageCap> checkDeviceCaps(CZBitset<RImageCap> caps, RDevice *device = nullptr) const noexcept = 0;

    /**
     * @brief Uploads pixel data into a region of the image.
     *
     * Increments writeSerial() on success.
     *
     * @param region Source buffer and set of rectangles to copy.
     * @return true on success, false otherwise.
     */
    virtual bool writePixels(const RPixelBufferRegion &region) noexcept = 0;

    /**
     * @brief Downloads pixel data from a region of the image.
     *
     * @param region Destination buffer and set of rectangles to copy.
     * @return true on success, false otherwise.
     */
    virtual bool readPixels(const RPixelBufferRegion &region) noexcept = 0;

    /**
     * @brief Returns the image size in pixels.
     */
    SkISize size() const noexcept { return m_size; }

    /**
     * @brief Returns information about the image's pixel format.
     */
    const RFormatInfo &formatInfo() const noexcept { return *m_formatInfo; }

    /**
     * @brief Returns the set of formats the image can be read/sampled as.
     */
    const std::unordered_set<RFormat> &readFormats() const noexcept { return m_readFormats; };

    /**
     * @brief Returns the set of formats the image can be written to.
     */
    const std::unordered_set<RFormat> &writeFormats() const noexcept { return m_writeFormats; }

    /**
     * @brief Returns the image's alpha type.
     */
    SkAlphaType alphaType() const noexcept { return m_alphaType; }

    /**
     * @brief Returns the DRM format modifier of the image's storage.
     */
    RModifier modifier() const noexcept { return m_modifier; }

    /**
     * @brief Returns the device that allocated this image.
     */
    RDevice *allocator() const noexcept { return m_allocator; }

    /**
     * @brief Returns the RCore instance that owns this image.
     */
    std::shared_ptr<RCore> core() const noexcept { return m_core; }

    // Signalled when no longer being read
    /**
     * @brief Returns the read sync fence.
     *
     * Signalled when the image is no longer being read.
     */
    std::shared_ptr<RSync> readSync() noexcept { return m_readSync; }

    /**
     * @brief Sets the read sync fence.
     *
     * @param sync The fence signalled when reads on the image complete.
     */
    void setReadSync(std::shared_ptr<RSync> sync) noexcept { m_readSync = sync;}

    // Signalled when pending write operations end
    /**
     * @brief Returns the write sync fence.
     *
     * Signalled when pending write operations on the image complete.
     */
    std::shared_ptr<RSync> writeSync() noexcept { return m_writeSync; }

    /**
     * @brief Sets the write sync fence.
     *
     * @param sync The fence signalled when writes on the image complete.
     */
    void setWriteSync(std::shared_ptr<RSync> sync) noexcept { m_writeSync = sync; }

    // Increased by successful writePixels calls, initially 0
    /**
     * @brief Returns the write serial.
     *
     * Starts at 0 and is incremented by each successful writePixels() call.
     */
    UInt32 writeSerial() const noexcept { return m_writeSerial; }

    /**
     * @brief Attempts to cast this image to an RGLImage.
     *
     * @return A shared pointer to RGLImage if the active API is OpenGL, or nullptr otherwise.
     */
    std::shared_ptr<RGLImage> asGL() const noexcept;

    /**
     * @brief Attempts to cast this image to an RRSImage.
     *
     * @return A shared pointer to RRSImage if the active API is Raster, or nullptr otherwise.
     */
    std::shared_ptr<RRSImage> asRS() const noexcept;

    /**
     * @brief Attempts to cast this image to an RVKImage.
     *
     * @return A shared pointer to RVKImage if the active API is Vulkan, or nullptr otherwise.
     */
    std::shared_ptr<RVKImage> asVK() const noexcept;


    /**
     * @brief Destructor.
     */
    ~RImage() noexcept;

    // Reserved for Louvre
    std::shared_ptr<CZObjectBase> louvre;
protected:
    RImage(std::shared_ptr<RCore> core, RDevice *device, SkISize size, const RFormatInfo *formatInfo, SkAlphaType alphaType, RModifier modifiers) noexcept;
    SkISize m_size;
    UInt32 m_writeSerial {};
    const RFormatInfo *m_formatInfo;
    SkAlphaType m_alphaType;
    RModifier m_modifier;
    RDevice *m_allocator;
    std::unordered_set<RFormat> m_readFormats;
    std::unordered_set<RFormat> m_writeFormats;
    std::shared_ptr<RCore> m_core;
    std::weak_ptr<RImage> m_self;
    std::shared_ptr<RSync> m_readSync;
    std::shared_ptr<RSync> m_writeSync;

    // Stores DMA fds as GEM handles to reduce the number of open fds
    std::shared_ptr<RGBMBo> m_bo;
};

#endif // RIMAGE_H
