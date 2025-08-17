#ifndef CZ_RMATRIXUTILS_H
#define CZ_RMATRIXUTILS_H

#include <CZ/Ream/RPainter.h>
#include <CZ/Ream/Ream.h>
#include <CZ/CZTransform.h>
#include <CZ/skia/core/SkMatrix.h>

class CZ::RMatrixUtils
{
public:
    /// Returns a matrix that applies the transform to RSurface::dst(), producing framebuffer pixel-space coordinates
    static SkMatrix DstTransform(CZTransform transform, SkSize dstSize) noexcept;

    /// Virtual -> RImage -> NDC coords
    static SkMatrix VirtualToNDC(CZTransform transform, SkRect viewport, SkRect dst, SkISize imageSize, bool flipY) noexcept;

    /// Virtual -> RImage coords
    static SkMatrix VirtualToImage(CZTransform transform, SkRect viewport, SkRect dst) noexcept;

    /// Virtual -> RImage -> UV coords
    static SkMatrix VirtualToUV(SkRect dst, CZTransform srcTransform, SkScalar srcScale, SkRect srcRect, SkISize texSize) noexcept;

    /// RDrawImageInfo src rect -> SkCanvas->drawImageRect src rect
    static SkRect SkImageSrcRect(const RDrawImageInfo &image) noexcept;

    /// Flips/rotates SkImage dst, the inverse should be applied to SkCanvas->drawImageRect dstRect
    static SkMatrix SkImageDstTransform(const RDrawImageInfo &image) noexcept;
};

#endif // CZ_RMATRIXUTILS_H
