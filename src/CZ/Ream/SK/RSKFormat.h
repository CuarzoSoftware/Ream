#ifndef CZ_RSKFORMAT_H
#define CZ_RSKFORMAT_H

#include <CZ/skia/gpu/ganesh/gl/GrGLTypes.h>
#include <CZ/Ream/Ream.h>
#include <CZ/skia/core/SkColorType.h>
#include <CZ/skia/private/gpu/ganesh/GrTypesPriv.h>
#include <unordered_set>

namespace CZ
{
    namespace RSKFormat
    {
        // kUnknown_SkColorType if no match
        SkColorType FromDRM(RFormat format) noexcept;

        // DRM_FORMAT_INVALID if no match
        RFormat ToDRM(SkColorType format) noexcept;

        // Supported Skia formats
        const std::unordered_set<RFormat> &SupportedFormats() noexcept;
    }
};

#endif // CZ_RSKFORMAT_H
