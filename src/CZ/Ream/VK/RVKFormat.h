#ifndef CZ_RVKFORMAT_H
#define CZ_RVKFORMAT_H

#include <CZ/Ream/Ream.h>
#include <vulkan/vulkan_core.h>

namespace CZ
{
    /**
     * @brief DRM FourCC <-> VkFormat mapping.
     *
     * Complements SK/RSKFormat (DRM <-> SkColorType) and GL/RGLFormat (DRM <-> GL).
     * Only formats meaningful to the Vulkan backend (and understood by Skia's Vulkan
     * backend for render targets) are mapped. DRM formats are little-endian byte-order
     * codes, so the mapping matches on in-memory byte layout.
     */
    namespace RVKFormat
    {
        /**
         * @brief Returns the VkFormat matching a DRM format, or VK_FORMAT_UNDEFINED if unmapped.
         */
        VkFormat FromDRM(RFormat format) noexcept;

        /**
         * @brief Returns the DRM format matching a VkFormat, or DRM_FORMAT_INVALID if unmapped.
         */
        RFormat ToDRM(VkFormat format) noexcept;
    }
}

#endif // CZ_RVKFORMAT_H
