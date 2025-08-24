#include <CZ/Ream/RS/RRSPainter.h>
#include <CZ/Ream/RSurface.h>
#include <CZ/Ream/RImage.h>
#include <CZ/Ream/RMatrixUtils.h>
#include <CZ/Ream/SK/RSKColor.h>
#include <CZ/Ream/SK/RSKImageWrap.h>
#include <CZ/skia/core/SkPath.h>
#include <CZ/skia/core/SkColorFilter.h>
#include <CZ/skia/core/SkShader.h>

using namespace CZ;

static sk_sp<SkColorFilter> ColorFactor(SkColor4f f) noexcept
{
    if (f.fR != 1.f || f.fG != 1.f || f.fB != 1.f || f.fA != 1.f)
    {
        const float factor[20] {
            f.fR,   0,      0,      0,      0,
            0,      f.fG,   0,      0,      0,
            0,      0,      f.fB,   0,      0,
            0,      0,      0,      f.fA,   0
        };

        return SkColorFilters::Matrix(factor);
    }

    return {};
}

static SkMatrix CZShaderMatrix(SkRect srcRect, SkRect dstRect, CZTransform transform)
{
    SkMatrix m, t, f, s;

    // Normalize srcRect → [0,1]x[0,1]
    SkRect unit = SkRect::MakeWH(1, 1);
    m.setRectToRect(srcRect, unit, SkMatrix::kFill_ScaleToFit);

    switch (transform)
    {
    case CZTransform::Normal:
        t.setIdentity();
        break;

    case CZTransform::Rotated90:
        // Rotate around (0.5, 0.5), then shift back into [0,1]
        t.setRotate(90, 0.5f, 0.5f);
        break;

    case CZTransform::Rotated180:
        t.setRotate(180, 0.5f, 0.5f);
        break;

    case CZTransform::Rotated270:
        t.setRotate(270, 0.5f, 0.5f);
        break;

    case CZTransform::Flipped:
        t.setScale(-1, 1, 0.5f, 0.5f);
        break;

    case CZTransform::Flipped90:
        t.setRotate(90, 0.5f, 0.5f);
        f.setScale(-1, 1, 0.5f, 0.5f);
        t.postConcat(f);
        break;

    case CZTransform::Flipped180:
        t.setRotate(180, 0.5f, 0.5f);
        f.setScale(-1, 1, 0.5f, 0.5f);
        t.postConcat(f);
        break;

    case CZTransform::Flipped270:
        t.setRotate(270, 0.5f, 0.5f);
        f.setScale(-1, 1, 0.5f, 0.5f);
        t.postConcat(f);
        break;
    }

    m.postConcat(t);
    s.setRectToRect(unit, dstRect, SkMatrix::kFill_ScaleToFit);
    m.postConcat(s);
    return m;
}

static sk_sp<SkShader> MakeImageShader(const RDrawImageInfo &image) noexcept
{
    const auto srcRect { RMatrixUtils::SkImageSrcRect(image)};
    const auto dstRect { SkRect::Make(image.dst) };
    const auto imageMatrix { CZShaderMatrix(srcRect, dstRect, image.srcTransform) };
    const auto sampling { SkSamplingOptions(
        image.magFilter == RImageFilter::Linear ? SkFilterMode::kLinear : SkFilterMode::kNearest,
        image.minFilter == RImageFilter::Linear ? SkMipmapMode::kLinear : SkMipmapMode::kNearest) };
    return image.image->skImage()->makeShader(
        RImageWrapToSK(image.wrapS),
        RImageWrapToSK(image.wrapT),
        sampling, &imageMatrix);
}

bool RRSPainter::drawImage(const RDrawImageInfo &image, const SkRegion *region, const RDrawImageInfo *mask) noexcept
{
    const auto surface { m_surface };
    SkPath clip;

    switch (validateDrawImage(image, region, mask, surface, clip))
    {
        case ValRes::Ok:
            break;
        case ValRes::Noop:
            return true;
        case ValRes::Error:
            return false;
    }

    auto *c { surface->image()->skSurface()->getCanvas() };
    c->save();
    c->resetMatrix();
    c->setMatrix(RMatrixUtils::VirtualToImage(geometry().transform, geometry().viewport, geometry().dst));
    c->clipPath(clip);

    // Color factor
    sk_sp<SkColorFilter> colorFilter { ColorFactor(state().factor) };

    // Replace color
    if (state().options.has(Option::ReplaceImageColor))
    {
        SkColor tint { state().options.has(Option::ColorIsPremult) ? SKColorUnpremultiply(color()) : color() };

        if (colorFilter)
            colorFilter = colorFilter->makeComposed(SkColorFilters::Blend(tint, SkBlendMode::kSrcIn));
        else
            colorFilter = SkColorFilters::Blend(tint, SkBlendMode::kSrcIn);
    }

    auto shader { MakeImageShader(image) };

    if (mask)
        shader = SkShaders::Blend(SkBlendMode::kDstIn, shader, MakeImageShader(*mask));

    SkPaint p;
    p.setColorFilter(colorFilter);
    p.setAlphaf(opacity());
    p.setBlendMode(static_cast<SkBlendMode>(blendMode()));
    p.setShader(shader);
    c->drawRect(SkRect::Make(image.dst), p);
    c->restore();
    return true;
}

bool RRSPainter::drawColor(const SkRegion &region) noexcept
{
    if (blendMode() == RBlendMode::SrcOver && (SkColorGetA(color()) == 0 || factor().fA <= 0.f || opacity() <= 0.f))
        return true;

    const auto surface { m_surface };

    SkRegion clip { geometry().viewport.roundOut() };
    clip.op(region, SkRegion::kIntersect_Op);
    if (clip.isEmpty())
        return true;

    SkPath path;
    clip.getBoundaryPath(&path);

    auto *c { surface->image()->skSurface()->getCanvas() };
    c->save();
    c->setMatrix(RMatrixUtils::VirtualToImage(geometry().transform, geometry().viewport, geometry().dst));
    c->clipPath(path);

    auto unColor { SkColor4f::FromColor(state().options.has(Option::ColorIsPremult) ? SKColorUnpremultiply(color()) : color()) };
    unColor.fR *= state().factor.fR;
    unColor.fG *= state().factor.fG;
    unColor.fB *= state().factor.fB;
    unColor.fA *= state().factor.fA;
    c->drawColor(unColor, static_cast<SkBlendMode>(blendMode()));
    c->restore();
    return true;
}

RRSPainter::ValRes RRSPainter::validateDrawImage(const RDrawImageInfo &image, const SkRegion *region, const RDrawImageInfo *mask, std::shared_ptr<RSurface> surface, SkPath &outClip) noexcept
{
    if (blendMode() == RBlendMode::SrcOver && (factor().fA <= 0.f || opacity() <= 0.f))
        return ValRes::Noop;

    if (!surface || !surface->image())
    {
        RLog(CZError, CZLN, "Missing RSurface");
        return ValRes::Error;
    }

    if (!image.image || !image.image->skImage())
    {
        RLog(CZError, CZLN, "Invalid src image");
        return ValRes::Error;
    }

    if (mask && !mask->image)
    {
        RLog(CZError, CZLN, "Invalid mask");
        return ValRes::Error;
    }

    SkRegion clip { geometry().viewport.roundOut() };

    if (region)
        clip.op(*region, SkRegion::kIntersect_Op);

    if (mask)
        clip.op(mask->dst, SkRegion::kIntersect_Op);

    if (clip.isEmpty())
        return ValRes::Noop;

    clip.getBoundaryPath(&outClip);
    return ValRes::Ok;
}

bool RRSPainter::setGeometry(const RSurfaceGeometry &geometry) noexcept
{
    // TODO: optimize
    if (!geometry.isValid())
        return false;

    m_state.geometry = geometry;
    return true;
}
