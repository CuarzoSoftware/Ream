#ifndef CZ_RSKIMAGEWRAP_H
#define CZ_RSKIMAGEWRAP_H

#include <CZ/Ream/RImageWrap.h>
#include <CZ/skia/core/SkTileMode.h>

namespace CZ
{
    /**
     * @brief Maps a Ream RImageWrap mode to the equivalent Skia SkTileMode.
     *
     * ClampToBorder maps to SkTileMode::kDecal. Falls back to SkTileMode::kClamp for unknown values.
     *
     * @param wrap The Ream wrap mode.
     * @return The corresponding SkTileMode.
     */
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
