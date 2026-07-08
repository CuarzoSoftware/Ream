#ifndef CZ_RIMAGEWRAP_H
#define CZ_RIMAGEWRAP_H

namespace CZ
{
    /**
     * @brief Addressing mode used when sampling outside an image's bounds.
     */
    enum class RImageWrap
    {
        Repeat,         ///< Tiles the image, wrapping coordinates around.
        MirroredRepeat, ///< Tiles the image, mirroring it on every repeat.
        ClampToEdge,    ///< Clamps coordinates to the edge texels.
        ClampToBorder   ///< Uses a border color outside the image bounds.
    };
};

#endif // CZ_RIMAGEWRAP_H
