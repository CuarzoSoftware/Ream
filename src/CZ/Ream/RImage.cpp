#include <CZ/Ream/SK/RSKFormat.h>
#include <CZ/Ream/GL/RGLImage.h>
#include <CZ/Ream/RDMABufferInfo.h>
#include <CZ/Ream/RCore.h>
#include <CZ/Ream/RLog.h>

#include <CZ/skia/core/SkPixelRef.h>
#include <CZ/skia/core/SkCanvas.h>
#include <CZ/skia/core/SkStream.h>
#include <CZ/skia/core/SkImageInfo.h>
#include <CZ/skia/core/SkBitmap.h>
#include <CZ/skia/modules/svg/include/SkSVGDOM.h>

using namespace CZ;

static UInt32 count { 0 };

std::shared_ptr<RImage> RImage::Make(SkISize size, const RDRMFormat &format, RStorageType storageType, RDevice *allocator) noexcept
{
    auto core { RCore::Get() };

    assert(core);

    if (core->graphicsAPI() == RGraphicsAPI::GL)
        return RGLImage::Make(size, format, storageType, (RGLDevice*)allocator);

    return {};
}

std::shared_ptr<RImage> RImage::MakeFromPixels(const RPixelBufferInfo &info, const RDRMFormat &format, RStorageType storageType, RDevice *allocator) noexcept
{
    auto image { Make(info.size, format, storageType, allocator) };

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

std::shared_ptr<RImage> RImage::LoadFile(const std::filesystem::path &path, const RDRMFormat &format, SkISize size, RDevice *allocator) noexcept
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

    auto stream { SkMemoryStream::MakeFromFile(path.c_str()) };
    auto dom { SkSVGDOM::MakeFromStream(*stream) };

    if (!dom)
    {
        RLog(CZError, CZLN, "Failed to create SkSVGDOM from file: {}", path.c_str());
        return {};
    }

    SkSize finalSize
    {
        size.fWidth > 0 ? SkIntToScalar(size.width()) : dom->containerSize().width(),
        size.fHeight > 0 ? SkIntToScalar(size.height()) : dom->containerSize().height()
    };

    if (finalSize.fWidth <= 0 || finalSize.fHeight <= 0)
    {
        RLog(CZError, CZLN, "The image has invalid dimensions: {}", path.c_str());
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
        RLog(CZError, CZLN, "Failed to allocate SkBitmap pixels");
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
    return MakeFromPixels(pixInfo, format, RStorageType::Auto, allocator);
}

std::shared_ptr<RGLImage> RImage::asGL() const noexcept
{
    return std::dynamic_pointer_cast<RGLImage>(m_self.lock());
}

RImage::~RImage() noexcept
{
    RLog(CZTrace, "- ({}) RImage", --count);
}

RImage::RImage(std::shared_ptr<RCore> core, RDevice *device, SkISize size, const RFormatInfo *formatInfo, SkAlphaType alphaType, const std::vector<RModifier> &modifiers) noexcept :
    m_size(size),
    m_formatInfo(formatInfo),
    m_alphaType(alphaType),
    m_modifiers(modifiers),
    m_allocator(device),
    m_core(core)
{
    assert(formatInfo);
    assert(SkAlphaTypeIsValid(alphaType));
    assert(!size.isEmpty());
    assert(!modifiers.empty());
    assert(device);
    assert(core);
    RLog(CZTrace, "+ ({}) RImage", ++count);
}
