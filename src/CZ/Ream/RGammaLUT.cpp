#include <CZ/Ream/RGammaLUT.h>
#include <cmath>

using namespace CZ;

void CZ::RGammaLUT::fill(Float64 gamma, Float64 brightness, Float64 contrast) noexcept
{
    if (size() == 0)
        return;

    auto r { red()   };
    auto g { green() };
    auto b { blue()  };

    if (gamma <= 0.0)
        gamma = 0.1;

    Float64 n { (Float64)size() - 1 };
    Float64 val;

    for (UInt32 i = 0; i < size(); i++)
    {
        val = contrast * std::pow((Float64)i / n, 1.0 / gamma) + (brightness - 1);

        if (val > 1.0)
            val = 1.0;
        else if (val < 0.0)
            val = 0.0;

        r[i] = g[i] = b[i] = (UInt16)(UINT16_MAX * val);
    }
}

std::shared_ptr<RGammaLUT> RGammaLUT::Make(size_t size) noexcept
{
    return std::shared_ptr<RGammaLUT>(new RGammaLUT(size));
}

std::shared_ptr<RGammaLUT> RGammaLUT::MakeFilled(size_t size, Float64 gamma, Float64 brightness, Float64 contrast) noexcept
{
    auto lut { Make(size) };
    lut->fill(gamma, brightness, contrast);
    return lut;
}
