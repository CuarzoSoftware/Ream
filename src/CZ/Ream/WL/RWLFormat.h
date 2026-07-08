#ifndef CZ_RWLFORMAT_H
#define CZ_RWLFORMAT_H

#include <CZ/Ream/Ream.h>
#include <wayland-client-protocol.h>
#include <drm_fourcc.h>

namespace CZ
{
    /**
     * @brief Helper for converting between DRM pixel formats and Wayland @c wl_shm formats.
     */
    struct RWLFormat
    {
        /**
         * @brief Converts a DRM (FourCC) format to the equivalent @c wl_shm_format.
         *
         * The XRGB8888 and ARGB8888 formats are mapped to their dedicated @c wl_shm enumerators;
         * all other formats share the same numeric value and are cast through directly.
         */
        static constexpr wl_shm_format FromDRM(RFormat format) noexcept
        {
            switch (format)
            {
            case DRM_FORMAT_XRGB8888:
                return WL_SHM_FORMAT_XRGB8888;
            case DRM_FORMAT_ARGB8888:
                return WL_SHM_FORMAT_ARGB8888;
            default:
                return static_cast<wl_shm_format>(format);
            }
        }

        /**
         * @brief Converts a Wayland @c wl_shm_format to the equivalent DRM (FourCC) format.
         *
         * The XRGB8888 and ARGB8888 enumerators are mapped to their DRM FourCC counterparts;
         * all other formats share the same numeric value and are cast through directly.
         */
        static constexpr RFormat ToDRM(wl_shm_format format) noexcept
        {
            switch (format)
            {
            case WL_SHM_FORMAT_XRGB8888:
                return DRM_FORMAT_XRGB8888;
            case WL_SHM_FORMAT_ARGB8888:
                return DRM_FORMAT_ARGB8888;
            default:
                return static_cast<RFormat>(format);
            }
        }
    };
}

#endif // RWLFORMAT_H
