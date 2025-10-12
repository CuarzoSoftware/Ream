#include <CZ/Ream/RMatrixUtils.h>
#include <CZ/Ream/RImage.h>

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

SkMatrix RMatrixUtils::VirtualToImage(CZTransform transform, SkRect viewport, SkRect dst) noexcept
{
    SkSize size;

    SkScalar sX, sY, tX, tY;

    if (CZ::Is90Transform(transform))
    {
        size = SkSize::Make(dst.height(), dst.width());
        sX = dst.height() / viewport.width();
        sY = dst.width() / viewport.height();
        tY = -viewport.top() * sX;
        tX = -viewport.left() * sY;
    }
    else
    {
        size = SkSize::Make(dst.width(), dst.height());
        sX = dst.width() / viewport.width();
        sY = dst.height() / viewport.height();
        tX = -viewport.left() * sX;
        tY = -viewport.top() * sY;
    }

    return
        SkMatrix::MakeAll(
            sX,  0.f, tX,
            0.f, sY,  tY,
            0.f, 0.f, 1.f).
        postConcat(DstTransform(transform, size)).postTranslate(dst.x(), dst.y());
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

SkRect RMatrixUtils::SkImageSrcRect(const RDrawImageInfo &image) noexcept
{
    if (!image.image)
        return SkRect::MakeEmpty();

    const auto imgSize = SkSize::Make(image.image->size().width(), image.image->size().height());

    SkRect srcRect = image.src;
    srcRect.fLeft   *= image.srcScale;
    srcRect.fTop    *= image.srcScale;
    srcRect.fRight  *= image.srcScale;
    srcRect.fBottom *= image.srcScale;

    switch (image.srcTransform)
    {
    case CZTransform::Normal:
        break;

    case CZTransform::Rotated90:
        srcRect = SkRect::MakeLTRB(
            srcRect.top(),
            imgSize.height() - srcRect.right(),
            srcRect.bottom(),
            imgSize.height() - srcRect.left());
        break;

    case CZTransform::Rotated180:
        srcRect = SkRect::MakeLTRB(
            imgSize.width() - srcRect.right(),
            imgSize.height() - srcRect.bottom(),
            imgSize.width() - srcRect.left(),
            imgSize.height() - srcRect.top());
        break;

    case CZTransform::Rotated270:
        srcRect = SkRect::MakeLTRB(
            imgSize.width() - srcRect.bottom(),
            srcRect.left(),
            imgSize.width() - srcRect.top(),
            srcRect.right());
        break;

    case CZTransform::Flipped:
        srcRect = SkRect::MakeLTRB(
            imgSize.width() - srcRect.right(),
            srcRect.top(),
            imgSize.width() - srcRect.left(),
            srcRect.bottom());
        break;

    case CZTransform::Flipped90:
        srcRect = SkRect::MakeLTRB(
            srcRect.top(),
            srcRect.left(),
            srcRect.bottom(),
            srcRect.right());
        break;

    case CZTransform::Flipped180:
        srcRect = SkRect::MakeLTRB(
            srcRect.left(),
            imgSize.height() - srcRect.bottom(),
            srcRect.right(),
            imgSize.height() - srcRect.top());
        break;

    case CZTransform::Flipped270:
        srcRect = SkRect::MakeLTRB(
            imgSize.height() - srcRect.bottom(),
            imgSize.height() - srcRect.right(),
            imgSize.height() - srcRect.top(),
            imgSize.height() - srcRect.left());
        break;
    }

    return srcRect;
}

SkMatrix RMatrixUtils::SkImageDstTransform(const RDrawImageInfo &image) noexcept
{
    SkMatrix matrix;
    matrix.setIdentity();

    if (image.dst.isEmpty())
    {
        matrix.setScale(0.f, 0.f);
        return matrix;
    }

    SkRect rect { SkRect::Make(image.dst) };
    const float width = rect.width();
    const float height = rect.height();
    const float centerX = rect.centerX();
    const float centerY = rect.centerY();
    const float aspectRatio = height / width;

    switch (image.srcTransform) {
    case CZTransform::Normal:
        // No transformation needed
        break;

    case CZTransform::Rotated90:
        matrix.preTranslate(centerX, centerY);
        matrix.preRotate(90);
        matrix.preScale(aspectRatio, 1.0f / aspectRatio);
        matrix.preTranslate(-centerY, -centerX);
        break;

    case CZTransform::Rotated180:
        matrix.preTranslate(centerX, centerY);
        matrix.preRotate(180);
        matrix.preTranslate(-centerX, -centerY);
        break;

    case CZTransform::Rotated270:
        matrix.preTranslate(centerX, centerY);
        matrix.preRotate(270);
        matrix.preScale(aspectRatio, 1.0f / aspectRatio);
        matrix.preTranslate(-centerY, -centerX);
        break;

    case CZTransform::Flipped:
        matrix.preTranslate(centerX, centerY);
        matrix.preScale(-1, 1);
        matrix.preTranslate(-centerX, -centerY);
        break;

    case CZTransform::Flipped90:
        matrix.preTranslate(centerX, centerY);
        matrix.preScale(-1, 1);
        matrix.preRotate(90);
        matrix.preScale(aspectRatio, 1.0f / aspectRatio);
        matrix.preTranslate(-centerY, -centerX);
        break;

    case CZTransform::Flipped180:
        matrix.preTranslate(centerX, centerY);
        matrix.preScale(1, -1);
        matrix.preTranslate(-centerX, -centerY);
        break;

    case CZTransform::Flipped270:
        matrix.preTranslate(centerX, centerY);
        matrix.preScale(-1, 1);
        matrix.preRotate(270);
        matrix.preScale(aspectRatio, 1.0f / aspectRatio);
        matrix.preTranslate(-centerY, -centerX);
        break;
    }

    return matrix;
}

