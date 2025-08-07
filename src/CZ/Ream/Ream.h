#ifndef CZ_REAM_H
#define CZ_REAM_H

#include <CZ/Cuarzo.h>
#include <string_view>

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

    std::string_view RGraphicsAPIString(RGraphicsAPI api) noexcept;

    enum class RPlatform
    {
        DRM,
        Wayland
    };

    std::string_view RPlatformString(RPlatform platform) noexcept;

    class RDRMFormat;
    class RDRMFramebuffer;
    class RDumbBuffer;
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
    class RMatrixUtils;
    struct RPresentationTime;
    struct RDMABufferInfo;

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
}

#endif
