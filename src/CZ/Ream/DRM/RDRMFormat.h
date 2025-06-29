#ifndef RDRMFORMAT_H
#define RDRMFORMAT_H

#include <CZ/Ream/Ream.h>
#include <CZ/Utils/CZMathUtils.h>

namespace CZ
{
    struct RFormatInfo
    {
        /**
         * @brief Number of bytes required to store one block of pixels.
         *
         * For uncompressed formats with 1x1 blocks, this typically equals the number of bytes per pixel.
         */
        UInt32 bytesPerBlock;

        /**
         * @brief Width of one block of pixels.
         *
         * The number of pixels wide in a single block for this image format (1 for uncompressed formats).
         * For example, in a format with a blockWidth of 4, each block covers 4 pixels horizontally.
         * This is important for formats that compress or group pixels in blocks rather than individually.
         */
        UInt32 blockWidth;

        /**
         * @brief Height of one block of pixels.
         *
         * The number of pixels tall in a single block for this image format (1 for uncompressed formats).
         * For example, if blockHeight is 2, each block covers 2 pixels vertically.
         * This matters for formats that group or compress pixels in blocks instead of individually.
         */
        UInt32 blockHeight;

        /**
         * @brief Bits per pixel.
         */
        UInt32 bpp;

        /**
         * @brief Number of bits that are actually used to represent color information — often excluding alpha or padding bits.
         */
        UInt32 depth;

        /**
         * @brief Number of separate memory planes used to store pixel data.
         *
         * Typically 1 for packed RGB formats, 2–3 for planar YUV formats.
         */
        UInt32 planes;

        /**
         * @brief `true` if the format includes an alpha channel.
         */
        bool alpha;

        /**
         * @brief Returns the total number of pixels covered by a single block.
         *
         * This is calculated as blockWidth × blockHeight.
         */
        constexpr UInt32 pixelsPerBlock() const noexcept
        {
            return blockWidth * blockHeight;
        }

        /**
         * @brief Computes the number of blocks required to cover a given image width.
         *
         * This uses ceiling division to ensure partial blocks are counted.
         * Important for compressed formats where data is grouped in blocks of multiple pixels.
         *
         * @param width Image width in pixels.
         * @return Number of horizontal blocks needed to cover the width.
         */
        constexpr UInt32 blocksPerRow(UInt32 width) const noexcept
        {
            return CZMathUtils::DivCeil(width, blockWidth);
        }

        /**
         * @brief Calculates the minimum required stride (in bytes) for a given image width.
         *
         * The stride must be at least this value to ensure a full row of image data can be represented in memory.
         * This value is based on the number of blocks per row and the number of bytes per block.
         *
         * @param width Image width in pixels.
         * @return Minimum stride in bytes needed to accommodate the image width.
         */
        constexpr UInt32 minStride(UInt32 width) const noexcept
        {
            return blocksPerRow(width) * bytesPerBlock;
        }

        /**
         * @brief Validates whether the provided stride matches the alignment and is sufficient for the image width.
         *
         * @param width Image width in pixels.
         * @param stride Stride in bytes.
         * @return `true` if stride is valid; `false` otherwise.
         */
        constexpr bool validateStride(UInt32 width, UInt32 stride) const noexcept
        {
            return (stride % bytesPerBlock) == 0 && stride >= minStride(width);
        }
    };

    struct RDRMFormat
    {
        static const RFormatInfo *GetInfo(RFormat format) noexcept;
        static RFormat GetAlphaSubstitute(RFormat format) noexcept;

        RFormat format;
        RModifier modifier;
    };
}

#endif // RDRMFORMAT_H
