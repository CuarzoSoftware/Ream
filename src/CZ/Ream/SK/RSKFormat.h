#ifndef CZ_RSKFORMAT_H
#define CZ_RSKFORMAT_H

#include <CZ/Ream/Ream.h>
#include <CZ/skia/core/SkColorType.h>

namespace CZ
{
    namespace RSKFormat
    {
        // kUnknown_SkColorType if no match
        SkColorType FromDRM(RFormat format) noexcept;

        // DRM_FORMAT_INVALID if no match
        RFormat ToDRM(SkColorType format) noexcept;
    }
};

#endif // CZ_RSKFORMAT_H
