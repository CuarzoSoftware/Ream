#ifndef CZ_RSKIMAGEWRAP_H
#define CZ_RSKIMAGEWRAP_H

#include <CZ/Ream/RImageWrap.h>
#include <CZ/skia/core/SkTileMode.h>

namespace CZ
{
    inline constexpr SkTileMode RImageWrapToSK(RImageWrap wrap) noexcept
    {
        switch (wrap)
        {
        case RImageWrap::Repeat:
            return SkTileMode::kRepeat;
        case RImageWrap::MirroredRepeat:
            return SkTileMode::kMirror;
        case RImageWrap::ClampToEdge:
            return SkTileMode::kClamp;
        case RImageWrap::ClampToBorder:
            return SkTileMode::kDecal;
        }

        return SkTileMode::kClamp;
    }
}

#endif // CZ_RSKIMAGEWRAP_H
