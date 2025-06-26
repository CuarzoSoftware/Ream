#ifndef REAM_H
#define REAM_H

#include <CZ/CZ.h>

namespace CZ
{
    /**
     * @brief DRM Four Characters Code Format.
     */
    using RFormat = UInt32;

    /**
     * @brief DRM Four Characters Code Modifier.
     */
    using RModifier = UInt64;

    class RObject;

    class RPlatformHandle;
    class RWLPlatformHandle;

    class RCore;
    class RDevice;
    class RImage;

    class RGLCore;
    class RGLDevice;
    class RGLImage;

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
}

    struct gbm_bo;

#endif
