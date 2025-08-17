#ifndef CZ_RBLENDMODE_H
#define CZ_RBLENDMODE_H

#include <CZ/skia/core/SkBlendMode.h>

namespace CZ
{
    enum class RBlendMode
    {
        Src = static_cast<int>(SkBlendMode::kSrc),
        SrcOver = static_cast<int>(SkBlendMode::kSrcOver),
        DstIn = static_cast<int>(SkBlendMode::kDstIn)
    };
};

#endif // CZ_RBLENDMODE_H
