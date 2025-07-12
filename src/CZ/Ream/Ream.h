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
    class RGBMBo;

    class RCore;
    class RGLCore;

    class RDevice;
    class RGLDevice;

    class RImage;
    class RGLImage;

    class RPainter;
    class RGLPainter;

    class RSync;
    class RGLSync;

    class RSurface;
    struct RPresentationTime;

    class RGLStrings;
    class RGLMakeCurrent;
    class RGLContextData;
    class RGLContextDataManager;
    struct RGLThreadDataManager;

    class REGLImage;

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
