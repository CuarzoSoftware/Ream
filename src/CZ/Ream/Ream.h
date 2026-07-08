#ifndef CZ_REAM_H
#define CZ_REAM_H

#include <CZ/Core/Cuarzo.h>
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

    /**
     * @brief Graphics backend used by Ream.
     */
    enum class RGraphicsAPI
    {
        Auto, ///< Let Ream choose the most suitable backend automatically.
        GL,   ///< OpenGL ES 2.0 backend.
        VK,   ///< Vulkan backend.
        RS    ///< Raster (CPU) backend.
    };

    /**
     * @brief Returns a human-readable string for the given graphics API.
     *
     * @param api The graphics API to convert.
     * @return A string view naming the API (e.g. "Vulkan"). Unknown values yield "Unknown".
     */
    std::string_view RGraphicsAPIString(RGraphicsAPI api) noexcept;

    /**
     * @brief Windowing/display platform Ream runs on.
     */
    enum class RPlatform
    {
        DRM,       ///< Direct Rendering Manager (bare KMS).
        Wayland,   ///< Wayland compositor.
        Offscreen  ///< Offscreen rendering (no display).
    };

    /**
     * @brief Returns a human-readable string for the given platform.
     *
     * @param platform The platform to convert.
     * @return A string view naming the platform (e.g. "Wayland"). Unknown values yield "Unknown".
     */
    std::string_view RPlatformString(RPlatform platform) noexcept;

    class RDRMFormat;
    class RDRMFramebuffer;
    class RDRMTimeline;
    class RDumbBuffer;
    class RGBMBo;

    class RPlatformHandle;
    class RWLPlatformHandle;
    class RDRMPlatformHandle;
    class ROFPlatformHandle;

    class RWLSwapchain;

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
    class RGammaLUT;
    class RSwapchain;
    struct RDMABufferInfo;

    // GL/EGL
    class RGLCore;
    class RGLDevice;
    class RGLImage;
    class RGLPainter;
    class RGLSync;
    class RGLShader;
    class RGLProgram;
    class RGLPass;
    class RGLSwapchainWL;

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
    class RVKPass;
    class RVKPipeline;
    class RVKSwapchainWL;

    class RVKExtensions;

    // Raster
    class RRSCore;
    class RRSDevice;
    class RRSImage;
    class RRSPainter;
    class RRSPass;
    class RRSSwapchainWL;
}

#endif
