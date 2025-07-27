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
#include <CZ/CZWeak.h>
#include <filesystem>
#include <memory>
#include <unordered_set>

namespace CZ
{
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

        static constexpr UInt8 *AddressAt(UInt8 *origin, SkIPoint offset, UInt32 bytesPerPixel, UInt32 stride) noexcept
        {
            return origin + (offset.y() * stride) + (offset.x() * bytesPerPixel);
        }
    };

    enum class RStorageType
    {
        Auto,
        Native,
        GBM
    };
}

class CZ::RImage : public RObject
{
public:
    /// Device-specific caps
    enum class DeviceCap
    {
        /// Can be used as a src image by an RPass (RPass::drawImage())
        RPassSrc,

        /// Can be used as a src image by an RSKPass (has an SkImage)
        RSKPassSrc,

        /// Can be used as a render destination in an RPass (accepted by RSurface::WrapImage)
        RPassDst,

        /// Can be used as a render destination in an RSKPass (has an SkSurface)
        RSKPassDst
    };

    [[nodiscard]] static std::shared_ptr<RImage> Make(SkISize size, const RDRMFormat &format, RStorageType storageType = RStorageType::Auto, RDevice *allocator = nullptr) noexcept;
    [[nodiscard]] static std::shared_ptr<RImage> MakeFromPixels(const RPixelBufferInfo &info, const RDRMFormat &format, RStorageType storageType = RStorageType::Auto, RDevice *allocator = nullptr) noexcept;
    [[nodiscard]] static std::shared_ptr<RImage> LoadFile(const std::filesystem::path &path, const RDRMFormat &format, SkISize size = {0, 0}, RDevice *allocator = nullptr) noexcept;

    virtual std::shared_ptr<RGBMBo> gbmBo(RDevice *device = nullptr) const noexcept = 0;
    virtual std::shared_ptr<RDRMFramebuffer> drmFb(RDevice *device = nullptr) const noexcept = 0;
    virtual sk_sp<SkImage> skImage(RDevice *device = nullptr) const noexcept = 0;
    virtual sk_sp<SkSurface> skSurface(RDevice *device = nullptr) const noexcept = 0;
    virtual bool checkDeviceCap(DeviceCap cap, RDevice *device = nullptr) const noexcept = 0;

    virtual bool writePixels(const RPixelBufferRegion &region) noexcept = 0;
    virtual bool readPixels(const RPixelBufferRegion &region) noexcept = 0;

    SkISize size() const noexcept
    {
        return m_size;
    }

    const RFormatInfo &formatInfo() const noexcept { return *m_formatInfo; }

    const std::unordered_set<RFormat> &readFormats() const noexcept { return m_readFormats; };
    const std::unordered_set<RFormat> &writeFormats() const noexcept { return m_writeFormats; }

    SkAlphaType alphaType() const noexcept
    {
        return m_alphaType;
    }

    // For each plane
    const std::vector<RModifier> &modifiers() const noexcept
    {
        return m_modifiers;
    }

    RDevice *allocator() const noexcept
    {
        return m_allocator;
    }

    RCore &core() const noexcept
    {
        return *m_core.get();
    }

    void setReadSync(std::shared_ptr<RSync> sync) noexcept
    {
        m_readSync = sync;
    }

    // Signaled when no longer beind read
    std::shared_ptr<RSync> readSync() noexcept
    {
        return m_readSync;
    }

    void setWriteSync(std::shared_ptr<RSync> sync) noexcept
    {
        m_writeSync = sync;
    }

    // Signaled when pending write operations end
    std::shared_ptr<RSync> writeSync() noexcept
    {
        return m_writeSync;
    }

    std::shared_ptr<RGLImage> asGL() const noexcept;

    ~RImage() noexcept;

protected:
    RImage(std::shared_ptr<RCore> core, RDevice *device, SkISize size, const RFormatInfo *formatInfo, SkAlphaType alphaType, const std::vector<RModifier> &modifiers) noexcept;
    SkISize m_size;
    const RFormatInfo *m_formatInfo;
    SkAlphaType m_alphaType;
    std::vector<RModifier> m_modifiers;
    RDevice *m_allocator;
    std::unordered_set<RFormat> m_readFormats;
    std::unordered_set<RFormat> m_writeFormats;
    std::shared_ptr<RCore> m_core;
    std::weak_ptr<RImage> m_self;
    std::shared_ptr<RSync> m_readSync;
    std::shared_ptr<RSync> m_writeSync;
    std::optional<RDMABufferInfo> m_dmaInfo;
    CZOwnership m_dmaInfoOwn;
};

#endif // RIMAGE_H
