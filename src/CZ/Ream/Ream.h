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

    enum class RGraphicsAPI
    {
        Auto,
        GL,
        VK
    };

    enum class RPlatform
    {
        DRM,
        Wayland
    };

    class RDRMFormat;
    class RDRMFramebuffer;
    class RGBMBo;

    class RPlatformHandle;
    class RWLPlatformHandle;
    class RDRMPlatformHandle;

    // Base
    class RObject;
    class RCore;
    class RDevice;
    class RImage;
    class RPainter;
    class RSync;
    class RSurface;
    class RPass;
    struct RPresentationTime;

    // Skia
    class RSKPass;

    // GL/EGL
    class RGLCore;
    class RGLDevice;
    class RGLImage;
    class RGLPainter;
    class RGLSync;
    class RGLShader;
    class RGLProgram;

    class RGLStrings;
    class RGLMakeCurrent;
    class RGLContextData;
    class RGLContextDataManager;
    struct RGLThreadDataManager;
    class REGLImage;

    // Vulkan
    class RVKCore;
    class RVKDevice;
    class RVKImage;
    class RVKPainter;
    class RVKSync;

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
