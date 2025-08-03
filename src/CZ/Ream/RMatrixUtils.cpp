#include <CZ/Ream/RMatrixUtils.h>

using namespace CZ;

SkMatrix RMatrixUtils::DstTransform(CZTransform transform, SkSize dstSize) noexcept
{
    switch (transform)
    {
    case CZTransform::Normal:
        return SkMatrix();
    case CZTransform::Rotated90:
        return SkMatrix::MakeAll(
            0, 1, 0,
            -1, 0, dstSize.width(),
            0, 0, 1 );
    case CZTransform::Rotated180:
        return SkMatrix::MakeAll(
            -1, 0, dstSize.width(),
            0,-1, dstSize.height(),
            0, 0, 1);
    case CZTransform::Rotated270:
        return SkMatrix::MakeAll(
            0, -1, dstSize.height(),
            1,  0, 0,
            0,  0, 1);
    case CZTransform::Flipped:
        return SkMatrix::MakeAll(
            -1, 0, dstSize.width(),
            0, 1, 0,
            0, 0, 1);
    case CZTransform::Flipped90:
        return SkMatrix::MakeAll(
            0, 1, 0,
            1, 0, 0,
            0, 0, 1);
    case CZTransform::Flipped180:
        return SkMatrix::MakeAll(
            1,  0, 0,
            0, -1, dstSize.height(),
            0,  0, 1);
    case CZTransform::Flipped270:
        return SkMatrix::MakeAll(
            0, -1, dstSize.height(),
            -1, 0,  dstSize.width(),
            0,  0, 1);
        break;
    }
    return SkMatrix();
}

SkMatrix RMatrixUtils::VirtualToNDC(CZTransform transform, SkRect viewport, SkRect dst, SkISize imageSize, bool flipY) noexcept
{
    SkMatrix mat { VirtualToImage(transform, viewport, dst) };

    if (flipY)
        mat.postConcat(SkMatrix::MakeAll(
            1, 0, 0,
            0,-1, imageSize.height(),
            0, 0, 1));

    return mat.postConcat(SkMatrix::MakeAll(
        2.0f / imageSize.width(), 0,                         -1,
        0,                        2.0f / imageSize.height(), -1,
        0,                        0,                          1));
}

SkMatrix RMatrixUtils::VirtualToImage(CZTransform transform, SkRect viewport, SkRect dst) noexcept
{
    const auto sX { dst.width() / viewport.width() };
    const auto sY { dst.height() / viewport.height() };
    const auto tX { dst.left() - viewport.left() * sX };
    const auto tY { dst.top() - viewport.top() * sY };

    return
        SkMatrix::MakeAll(
            sX,  0.f, tX,
            0.f, sY,  tY,
            0.f, 0.f, 1.f).
        postConcat(DstTransform(transform, SkSize::Make(dst.width(), dst.height())));
}

SkMatrix RMatrixUtils::VirtualToUV(SkRect dst, CZTransform srcTransform, SkScalar srcScale, SkRect srcRect, SkISize texSize) noexcept
{
    const auto sX { srcRect.width() / dst.width() };
    const auto sY { srcRect.height() / dst.height() };
    const auto tX { srcRect.left() - dst.left() * sX };
    const auto tY { srcRect.top()  - dst.top()  * sY };

    SkMatrix dstToSrcVirtual;
    dstToSrcVirtual.setAll(
        sX, 0,  tX,
        0,  sY, tY,
        0,  0,  1);

    SkMatrix virtToPixel;
    virtToPixel.setAll(
        srcScale, 0,        0,
        0,        srcScale, 0,
        0,        0,        1
        );

    SkMatrix M { dstToSrcVirtual };
    M.postConcat(virtToPixel);

    SkRect srcPixelRect;
    virtToPixel.mapRect(&srcPixelRect, srcRect);

    SkMatrix cz = DstTransform(srcTransform,
        CZ::Is90Transform(srcTransform) ? SkSize::Make(texSize.height(),texSize.width()) : SkSize::Make(texSize));
    M.postConcat(cz);

    SkMatrix toUV;
    toUV.setAll(
        1.0f / texSize.width(),  0,                        0,
        0,                       1.0f / texSize.height(),  0,
        0,                       0,                        1);
    M.postConcat(toUV);

    return M;
}
