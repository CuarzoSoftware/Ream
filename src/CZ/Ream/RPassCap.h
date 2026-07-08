#ifndef CZ_RPASSCAP_H
#define CZ_RPASSCAP_H

namespace CZ
{
    /**
     * @brief Rendering capabilities that can be enabled for an RPass.
     *
     * These flags are combined into a bitset when beginning a pass (see RSurface::beginPass())
     * to select which drawing APIs may be used to render into the surface.
     */
    enum RPassCap
    {
        RPassCap_Painter = 1 << 0,  ///< Enables rendering through RPainter.
        RPassCap_SkCanvas = 1 << 1  ///< Enables rendering through an SkCanvas.
    };
};

#endif // CZ_RPASSCAP_H
