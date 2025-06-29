#ifndef RIMAGE_H
#define RIMAGE_H

#include <CZ/Ream/RObject.h>
#include <CZ/Ream/DRM/RDRMFormat.h>
#include <CZ/skia/core/SkSize.h>
#include <CZ/skia/core/SkRegion.h>
#include <CZ/CZWeak.h>
#include <memory>

namespace CZ
{
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
    };

    /**
     * @brief Describes a region to be copied from a pixel buffer.
     *
     * Specifies a set of rectangles and an offset for copying pixel data between buffers.
     */
    struct RPixelBufferRegion
    {
        /**
         * @brief Offset applied to each source rect before copying.
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
         * @brief Stride in bytes of the source pixel buffer.
         */
        UInt32 stride;

        /**
         * @brief Pointer to the top-left corner of the source pixel data.
         */
        UInt8 *pixels;

        /**
         * @brief Set of rectangles to copy, relative to the top-left corner of the source buffer.
         */
        SkRegion region;

        static constexpr UInt8 *AddressAt(UInt8 *origin, SkIPoint offset, UInt32 bytesPerPixel, UInt32 stride) noexcept
        {
            return origin + (offset.y() * stride) + (offset.x() * bytesPerPixel);
        }
    };

    struct RDMABufferInfo
    {

    };
}

class CZ::RImage : public RObject
{
public:

    [[nodiscard]] static std::shared_ptr<RImage> MakeFromPixels(const RPixelBufferInfo &params, RDevice *allocator = nullptr) noexcept;
    //static std::shared_ptr<RImage> MakeFromGBM(gbm_bo *bo, CZOwnership ownership) noexcept;

    virtual bool writePixels(const RPixelBufferRegion &region) noexcept = 0;

    SkISize size() const noexcept
    {
        return m_size;
    }

    const RDRMFormat &format() const noexcept
    {
        return m_format;
    }

    RDevice *allocator() const noexcept
    {
        return m_allocator;
    }

    RCore &core() const noexcept
    {
        return *m_core.get();
    }

    std::shared_ptr<RGLImage> asGL() const noexcept;

protected:
    RImage(std::shared_ptr<RCore> core, RDevice *device, SkISize size, const RDRMFormat &format) noexcept;
    SkISize m_size;
    RDRMFormat m_format;
    RDevice *m_allocator;
    std::shared_ptr<RCore> m_core;
    std::weak_ptr<RImage> m_self;
};

#endif // RIMAGE_H
