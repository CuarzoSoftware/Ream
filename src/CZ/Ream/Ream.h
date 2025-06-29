#ifndef REAM_H
#define REAM_H

#include <CZ/CZ.h>

namespace CZ
{
    /**
     * @brief DRM Four Characters Code Format.
     *
     * @see <drm_fourcc.h>
     */
    using RFormat = UInt32;

    /**
     * @brief DRM Four Characters Code Modifier.
     *
     * @see <drm_fourcc.h>
     */
    using RModifier = UInt64;

    class RObject;

    class RPlatformHandle;
    class RWLPlatformHandle;

    class RCore;
    class RDevice;
    class RImage;
    class RSurface;
    class RPainter;

    class RGLCore;
    class RGLDevice;
    class RGLImage;
    class RGLPainter;
    class RGLStrings;
    class RGLMakeCurrent;

    enum class RGraphicsAPI
    {
        Auto,
        GL,
        Vk
    };

    enum class RPlatform
    {
        DRM,
        Wayland
    };

    enum class RFilterMode
    {
        Nearest,
        Linear
    };
}

    struct gbm_bo;

#endif
