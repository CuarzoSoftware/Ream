#ifndef CZ_RBLENDMODE_H
#define CZ_RBLENDMODE_H

#include <CZ/skia/core/SkBlendMode.h>

namespace CZ
{
    /**
     * @brief Blend modes supported by Ream.
     *
     * A subset of Skia's SkBlendMode; each value maps directly to its SkBlendMode counterpart.
     */
    enum class RBlendMode
    {
        Src = static_cast<int>(SkBlendMode::kSrc), ///< Replaces the destination with the source (no blending).
        SrcOver = static_cast<int>(SkBlendMode::kSrcOver), ///< Draws the source over the destination (standard alpha blending).
        DstIn = static_cast<int>(SkBlendMode::kDstIn) ///< Keeps the destination only where it overlaps the source (masks by source alpha).
    };
};

#endif // CZ_RBLENDMODE_H
