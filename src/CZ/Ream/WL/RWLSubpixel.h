#ifndef CZ_RWLSUBPIXEL_H
#define CZ_RWLSUBPIXEL_H

#include <CZ/Ream/RSubpixel.h>
#include <wayland-server-protocol.h>

namespace CZ
{
    struct RWLSubpixel
    {
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
