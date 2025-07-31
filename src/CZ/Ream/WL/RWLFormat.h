#ifndef CZ_RWLFORMAT_H
#define CZ_RWLFORMAT_H

#include <CZ/Ream/Ream.h>
#include <wayland-client-protocol.h>
#include <drm_fourcc.h>

namespace CZ
{
    struct RWLFormat
    {
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
