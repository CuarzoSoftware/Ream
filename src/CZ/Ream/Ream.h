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
    class RDRMPlatformHandle;
    class RDRMFormat;
    class RDRMFramebuffer;

    class REGLImage;

    class RCore;
    class RDevice;
    class RImage;
    class RSurface;
    class RPainter;
    class RGBMBo;
    struct RPresentationTime;

    class RGLCore;
    class RGLDevice;
    class RGLImage;
    class RGLPainter;
    class RGLStrings;
    class RGLMakeCurrent;
    class RGLContextData;
    class RGLContextDataManager;
    struct RGLThreadDataManager;

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

    struct RDMABufferInfo
    {
        Int32 width;
        Int32 height;
        RFormat format;
        RModifier modifier;
        int planeCount;
        UInt32 offset[4] {};
        UInt32 stride[4] {};
        int fd[4] { -1, -1, -1, -1 };
    };

}

    struct gbm_bo;

#endif
