#ifndef CZ_RWLSUBPIXEL_H
#define CZ_RWLSUBPIXEL_H

#include <CZ/Ream/RSubpixel.h>
#include <wayland-server-protocol.h>

namespace CZ
{
    /**
     * @brief Helper for converting between Ream (RSubpixel) and Wayland (@c wl_output_subpixel) subpixel layouts.
     */
    struct RWLSubpixel
    {
        /**
         * @brief Converts an RSubpixel value to the equivalent @c wl_output_subpixel.
         *
         * Unrecognized values map to WL_OUTPUT_SUBPIXEL_UNKNOWN.
         */
        static constexpr wl_output_subpixel FromReam(RSubpixel subpixel) noexcept
        {
            switch (subpixel) {
            case RSubpixel::Unknown:
                return WL_OUTPUT_SUBPIXEL_UNKNOWN;
            case RSubpixel::HRGB:
                return WL_OUTPUT_SUBPIXEL_HORIZONTAL_RGB;
            case RSubpixel::HBGR:
                return WL_OUTPUT_SUBPIXEL_HORIZONTAL_BGR;
            case RSubpixel::VRGB:
                return WL_OUTPUT_SUBPIXEL_VERTICAL_RGB;
            case RSubpixel::VBGR:
                return WL_OUTPUT_SUBPIXEL_VERTICAL_BGR;
            case RSubpixel::None:
                return WL_OUTPUT_SUBPIXEL_NONE;
            default:
                return WL_OUTPUT_SUBPIXEL_UNKNOWN;
            }
        };

        /**
         * @brief Converts a @c wl_output_subpixel value to the equivalent RSubpixel.
         *
         * Unrecognized values map to RSubpixel::Unknown.
         */
        static constexpr RSubpixel ToReam(wl_output_subpixel subpixel) noexcept
        {
            switch (subpixel) {
            case WL_OUTPUT_SUBPIXEL_UNKNOWN:
                return RSubpixel::Unknown;
            case WL_OUTPUT_SUBPIXEL_HORIZONTAL_RGB:
                return RSubpixel::HRGB;
            case WL_OUTPUT_SUBPIXEL_HORIZONTAL_BGR:
                return RSubpixel::HBGR;
            case WL_OUTPUT_SUBPIXEL_VERTICAL_RGB:
                return RSubpixel::VRGB;
            case WL_OUTPUT_SUBPIXEL_VERTICAL_BGR:
                return RSubpixel::VBGR;
            case WL_OUTPUT_SUBPIXEL_NONE:
                return RSubpixel::None;
            default:
                return RSubpixel::Unknown;
            }
        };
    };
}

#endif // CZ_RWLSUBPIXEL_H
