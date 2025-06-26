#ifndef RDRMFORMAT_H
#define RDRMFORMAT_H

#include <CZ/Ream/Ream.h>

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
