#ifndef CZ_RSKFORMAT_H
#define CZ_RSKFORMAT_H

#include <CZ/skia/gpu/ganesh/gl/GrGLTypes.h>
#include <CZ/Ream/Ream.h>
#include <CZ/skia/core/SkColorType.h>
#include <CZ/skia/private/gpu/ganesh/GrTypesPriv.h>
#include <unordered_set>

namespace CZ
{
    /**
     * @brief DRM FourCC <-> SkColorType mapping.
     *
     * Complements VK/RVKFormat (DRM <-> VkFormat) and GL/RGLFormat (DRM <-> GL). Only formats
     * that Skia understands are mapped.
     */
    namespace RSKFormat
    {
        /**
         * @brief Returns the SkColorType matching a DRM format, or kUnknown_SkColorType if unmapped.
         */
        SkColorType FromDRM(RFormat format) noexcept;

        /**
         * @brief Returns the DRM format matching an SkColorType, or DRM_FORMAT_INVALID if unmapped.
         */
        RFormat ToDRM(SkColorType format) noexcept;

        /**
         * @brief Returns the set of DRM formats supported by the Skia mapping.
         */
        const std::unordered_set<RFormat> &SupportedFormats() noexcept;
    }
};

#endif // CZ_RSKFORMAT_H
