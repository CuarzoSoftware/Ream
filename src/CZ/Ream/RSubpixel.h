#ifndef RSUBPIXEL_H
#define RSUBPIXEL_H

#include <CZ/Core/Cuarzo.h>
#include <algorithm>
#include <string_view>
#include <array>

namespace CZ
{
    /**
     * @brief Subpixel layouts.
     *
     * Matches DRM subpixel values.
     */
    enum class RSubpixel
    {
        /**
         * @brief Unknown subpixel layout.
         */
        Unknown = 1,

        /**
         * @brief Horizontal subpixel layout with RGB order.
         *
         * Red, green, and blue subpixels are arranged horizontally.
         */
        HRGB = 2,

        /**
         * @brief Horizontal subpixel layout with BGR order.
         *
         * Blue, green, and red subpixels are arranged horizontally.
         */
        HBGR = 3,

        /**
         * @brief Vertical subpixel layout with RGB order.
         *
         * Red, green, and blue subpixels are arranged vertically.
         */
        VRGB = 4,

        /**
         * @brief Vertical subpixel layout with BGR order.
         *
         * Blue, green, and red subpixels are arranged vertically.
         */
        VBGR = 5,

        /**
         * @brief No specific subpixel layout.
         *
         * The connector does not have a well-defined order for subpixels.
         */
        None = 6,
    };

    inline const std::string_view &RSubPixelString(RSubpixel subpixel) noexcept
    {
        static constexpr const std::array<std::string_view, 7> strings { "Unknown", "HRGB", "HBGR", "VRGB", "VBGR", "None", "Unknown" };
        return strings[std::clamp(static_cast<UInt32>(subpixel) - 1U, 0U, 6U)];
    }
};

#endif // RSUBPIXEL_H
