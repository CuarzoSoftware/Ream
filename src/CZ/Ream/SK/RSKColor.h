#ifndef CZ_RSKCOLOR_H
#define CZ_RSKCOLOR_H

#include <CZ/skia/core/SkColor.h>

namespace CZ
{
    constexpr SkColor SKColorUnpremultiply(SkColor premul) noexcept
    {
        const auto a { SkColorGetA(premul) };

        if (a == 0)
            return SkColorSetARGB(0, 0, 0, 0);

        const auto r { SkColorGetR(premul) };
        const auto g { SkColorGetG(premul) };
        const auto b { SkColorGetB(premul) };

        // Scale back to 0–255 range
        const auto rUn { (r * 255 + a / 2) / a };
        const auto gUn { (g * 255 + a / 2) / a };
        const auto bUn { (b * 255 + a / 2) / a };

        return SkColorSetARGB(a, rUn, gUn, bUn);
    }

    constexpr SkColor4f SKColor4fUnpremultiply(SkColor4f premul) noexcept
    {
        if (premul.fA == 0.0f)
            return {0, 0, 0, 0};

        return {
            premul.fR / premul.fA,
            premul.fG / premul.fA,
            premul.fB / premul.fA,
            premul.fA
        };
    }
}

#endif // CZ_RSKCOLOR_H
