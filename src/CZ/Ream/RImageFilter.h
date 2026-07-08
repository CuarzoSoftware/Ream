#ifndef CZ_RIMAGEFILTER_H
#define CZ_RIMAGEFILTER_H

namespace CZ
{
    /**
     * @brief Sampling filter used when scaling an image.
     */
    enum class RImageFilter
    {
        Linear,  ///< Bilinear interpolation between texels.
        Nearest  ///< Nearest-neighbour sampling (no interpolation).
    };
};

#endif // CZ_RIMAGEFILTER_H
