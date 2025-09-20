#include <CZ/Ream/SK/RSKFormat.h>
#include <CZ/Ream/GL/RGLImage.h>
#include <CZ/Ream/RDMABufferInfo.h>
#include <CZ/Ream/RResourceTracker.h>
#include <CZ/Ream/RCore.h>
#include <CZ/Ream/RLog.h>

#include <CZ/Ream/RS/RRSImage.h>

#include <CZ/skia/core/SkPixelRef.h>
#include <CZ/skia/core/SkCanvas.h>
#include <CZ/skia/core/SkStream.h>
#include <CZ/skia/core/SkImageInfo.h>
#include <CZ/skia/core/SkBitmap.h>
#include <CZ/skia/core/SkImage.h>
#include <CZ/skia/codec/SkEncodedImageFormat.h>
#include <CZ/skia/codec/SkCodec.h>
#include <CZ/skia/modules/svg/include/SkSVGDOM.h>

using namespace CZ;

std::shared_ptr<RImage> RImage::Make(SkISize size, const RDRMFormat &format, const RImageConstraints *constraints) noexcept
{
    auto core { RCore::Get() };

    if (!core)
    {
        RLog(CZError, CZLN, "Missing RCore");
        return {};
    }

    if (core->graphicsAPI() == RGraphicsAPI::GL)
        return RGLImage::Make(size, format, constraints);
    else if (core->graphicsAPI() == RGraphicsAPI::RS)
        return RRSImage::Make(size, format, constraints);

    return {};
}

std::shared_ptr<RImage> RImage::MakeFromPixels(const RPixelBufferInfo &info, const RDRMFormat &format, const RImageConstraints *constraints) noexcept
{
    auto core { RCore::Get() };

    if (!core)
    {
        RLog(CZError, CZLN, "Missing RCore");
        return {};
    }

    RImageConstraints cons {};

    if (constraints)
        cons = *constraints;

    if (!cons.allocator)
        cons.allocator = core->mainDevice();

    cons.writeFormats.emplace(info.format);

    auto image { Make(info.size, format, &cons) };

    if (!image)
        return {};

    RPixelBufferRegion region {};
    region.pixels = info.pixels;
    region.region.setRect(SkIRect::MakeSize(info.size));
    region.format = info.format;
    region.stride = info.stride;

    if (!image->writePixels(region))
        return {};

    return image;
}

static std::shared_ptr<RImage> LoadImage(SkColorType skFormat, const std::filesystem::path &path, const RDRMFormat &format, SkISize size, const RImageConstraints *constraints) noexcept
{
    SkBitmap bitmap;
    SkSize finalSize = SkSize::Make(SkIntToScalar(size.width()), SkIntToScalar(size.height()));
    bool haveFinalSize = (size.width() > 0 && size.height() > 0);
    sk_sp<SkData> data = SkData::MakeFromFileName(path.c_str());

    if (!data)
    {
        RLog(CZDebug, CZLN, "Failed to open image file: {}", path.c_str());
        return {};
    }

    auto codec = SkCodec::MakeFromData(data);

    if (!codec)
    {
        RLog(CZDebug, CZLN, "Failed to create codec for image file: {}", path.c_str());
        return {};
    }

    SkISize srcSize = codec->dimensions();

    if (!haveFinalSize)
        finalSize.set(SkIntToScalar(srcSize.width()), SkIntToScalar(srcSize.height()));

    if (finalSize.fWidth <= 0 || finalSize.fHeight <= 0)
    {
        RLog(CZDebug, CZLN, "The image has invalid dimensions: {}", path.c_str());
        return {};
    }

    SkISize finalPixelSize = SkISize::Make(
        SkScalarCeilToInt(finalSize.fWidth),
        SkScalarCeilToInt(finalSize.fHeight)
    );

    // Decode into a temporary SkImage first (preserves original orientation/color),
    // then draw it into the target bitmap with scaling/conversion.
    sk_sp<SkImage> srcImage = SkImages::DeferredFromEncodedData(data);

    if (!srcImage)
    {
        RLog(CZDebug, CZLN, "Failed to decode image file: {}", path.c_str());
        return {};
    }

    SkImageInfo info = SkImageInfo::Make(
        finalPixelSize,
        skFormat,
        kPremul_SkAlphaType
        );

    if (!bitmap.tryAllocPixels(info)) {
        RLog(CZDebug, CZLN, "Failed to allocate SkBitmap pixels for raster image");
        return {};
    }

    SkCanvas canvas{ bitmap };
    canvas.clear(SK_ColorTRANSPARENT);

    // Draw the source image (full) into the target rect with filtering.
    SkRect srcRect = SkRect::MakeIWH(srcSize.width(), srcSize.height());
    SkRect dstRect = SkRect::MakeIWH(finalPixelSize.width(), finalPixelSize.height());
    SkPaint paint;
    paint.setAntiAlias(true);
    canvas.drawImageRect(srcImage, srcRect, dstRect, SkSamplingOptions(), &paint, SkCanvas::kFast_SrcRectConstraint);

    // At this point bitmap has the rendered image (SVG or raster), in desired format/size.
    if (!bitmap.pixelRef() || !bitmap.readyToDraw()) {
        RLog(CZDebug, CZLN, "Bitmap not ready after image processing: {}", path.c_str());
        return {};
    }

    const SkISize pixelSize{ bitmap.pixelRef()->width(), bitmap.pixelRef()->height() };

    RPixelBufferInfo pixInfo{};
    pixInfo.pixels = static_cast<UInt8*>(bitmap.pixelRef()->pixels());
    pixInfo.stride = bitmap.pixelRef()->rowBytes();
    pixInfo.format = format.format();
    pixInfo.size = pixelSize;
    return RImage::MakeFromPixels(pixInfo, format, constraints);
}

static std::shared_ptr<RImage> LoadSVG(SkColorType skFormat, const std::filesystem::path &path, const RDRMFormat &format, SkISize size, const RImageConstraints *constraints) noexcept
{
    auto stream { SkMemoryStream::MakeFromFile(path.c_str()) };

    if (!stream)
    {
        RLog(CZDebug, CZLN, "Failed to create SkMemoryStream from file: {}", path.c_str());
        return {};
    }

    auto dom { SkSVGDOM::MakeFromStream(*stream) };

    if (!dom)
    {
        RLog(CZDebug, CZLN, "Failed to create SkSVGDOM from file: {}", path.c_str());
        return {};
    }

    SkSize finalSize
    {
        size.fWidth > 0 ? SkIntToScalar(size.width()) : dom->containerSize().width(),
        size.fHeight > 0 ? SkIntToScalar(size.height()) : dom->containerSize().height()
    };

    if (finalSize.fWidth <= 0 || finalSize.fHeight <= 0)
    {
        RLog(CZDebug, CZLN, "The image has invalid dimensions: {}", path.c_str());
        return {};
    }

    SkImageInfo info
    {
        SkImageInfo::Make(
            finalSize.toCeil(),
            skFormat,
            kPremul_SkAlphaType)
    };

    SkBitmap bitmap;
    if (!bitmap.tryAllocPixels(info))
    {
        RLog(CZDebug, CZLN, "Failed to allocate SkBitmap pixels");
        return {};
    }

    SkCanvas canvas { bitmap };
    canvas.clear(SK_ColorTRANSPARENT);
    canvas.scale(
        finalSize.fWidth / dom->containerSize().width(),
        finalSize.fHeight / dom->containerSize().height());
    dom->render(&canvas);

    const SkISize pixelSize { bitmap.pixelRef()->width(), bitmap.pixelRef()->height() };

    RPixelBufferInfo pixInfo {};
    pixInfo.pixels = (UInt8*)bitmap.pixelRef()->pixels();
    pixInfo.stride = bitmap.pixelRef()->rowBytes();
    pixInfo.format = format.format();
    pixInfo.size = pixelSize;
    return RImage::MakeFromPixels(pixInfo, format, constraints);
}


std::shared_ptr<RImage> RImage::LoadFile(const std::filesystem::path &path, const RDRMFormat &format, SkISize size, const RImageConstraints *constraints) noexcept
{
    const SkColorType skFormat { RSKFormat::FromDRM(format.format()) };

    if (skFormat == kUnknown_SkColorType)
    {
        RLog(CZError, CZLN, "Could not find DRM -> SkColorType mapping");
        return {};
    }

    auto core { RCore::Get() };

    if (!core)
    {
        RLog(CZError, CZLN, "Missing RCore");
        return {};
    }

    std::shared_ptr<RImage> image { LoadImage(skFormat, path, format, size, constraints) };
    if (image) return image;
    image = LoadSVG(skFormat, path, format, size, constraints);

    if (!image)
        RLog(CZError, CZLN, "Failed to load image: {}", path.c_str());

    return image;
}

std::shared_ptr<RImage> RImage::FromDMA(const RDMABufferInfo &info, CZOwn ownership, const RImageConstraints *constraints) noexcept
{
    auto core { RCore::Get() };

    if (!core)
    {
        RLog(CZError, CZLN, "Missing RCore");
        goto fail;
    }

    if (core->graphicsAPI() == RGraphicsAPI::GL)
        return RGLImage::FromDMA(info, ownership, constraints);

fail:
    if (ownership == CZOwn::Own)
    {
        auto copy { info };
        copy.closeFds();
    }

    return {};
}

std::shared_ptr<RGLImage> RImage::asGL() const noexcept
{
    return std::dynamic_pointer_cast<RGLImage>(m_self.lock());
}

std::shared_ptr<RRSImage> RImage::asRS() const noexcept
{
    return std::dynamic_pointer_cast<RRSImage>(m_self.lock());
}

RImage::~RImage() noexcept
{
    if (m_dmaInfo.has_value() && m_dmaInfoOwn == CZOwn::Own)
        for (auto i = 0; i < m_dmaInfo->planeCount; i++)
            close(m_dmaInfo->fd[i]);

    RResourceTrackerSub(RResourceType::RImageRes);
}

RImage::RImage(std::shared_ptr<RCore> core, RDevice *device, SkISize size, const RFormatInfo *formatInfo, SkAlphaType alphaType, RModifier modifier) noexcept :
    m_size(size),
    m_formatInfo(formatInfo),
    m_alphaType(alphaType),
    m_modifier(modifier),
    m_allocator(device),
    m_core(core)
{
    assert(formatInfo);
    assert(SkAlphaTypeIsValid(alphaType));
    assert(!size.isEmpty());
    assert(device);
    assert(core);
    RResourceTrackerAdd(RResourceType::RImageRes);
}
