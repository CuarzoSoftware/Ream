#include <CZ/skia/gpu/ganesh/GrDirectContext.h>
#include <CZ/skia/gpu/ganesh/GrRecordingContext.h>
#include <CZ/Ream/RMatrixUtils.h>
#include <CZ/Ream/RResourceTracker.h>
#include <CZ/Ream/RLog.h>
#include <CZ/Ream/RSurface.h>
#include <CZ/Ream/RCore.h>
#include <CZ/Ream/RDevice.h>
#include <CZ/Ream/RPainter.h>
#include <CZ/Ream/RImage.h>
#include <CZ/Ream/RSync.h>
#include <CZ/Ream/RPass.h>

using namespace CZ;

std::shared_ptr<RSurface> RSurface::Make(SkISize size, SkScalar scale, bool alpha) noexcept
{
    SkISize imageSize (size.width() * scale, size.height() * scale);

    if (size.isEmpty() || imageSize.isEmpty())
    {
        RLog(CZWarning, CZLN, "Invalid dimensions ({},{}) or scale {}. Falling back to (1,1) and scale 1", size.width(), size.height(), scale);
        imageSize = size = {1, 1};
    }

    auto core { RCore::Get() };

    if (!core)
    {
        RLog(CZError, CZLN, "Missing RCore");
        return {};
    }

    RImageConstraints cons {};
    cons.allocator = core->mainDevice();
    cons.caps[cons.allocator] = RImageCap_Dst | RImageCap_Src | RImageCap_SkImage | RImageCap_SkSurface;
    std::shared_ptr<RImage> image;

    if (alpha)
        image = RImage::Make(imageSize, { DRM_FORMAT_ABGR8888, { DRM_FORMAT_MOD_INVALID } }, &cons);
    else
        image = RImage::Make(imageSize, { DRM_FORMAT_XBGR8888, { DRM_FORMAT_MOD_INVALID } }, &cons);

    if (!image)
    {
        RLog(CZError, CZLN, "Failed to create RImage");
        return {};
    }

    auto surface { std::shared_ptr<RSurface>(new RSurface(image)) };
    surface->m_self = surface;
    surface->m_geometry.viewport = SkRect::MakeWH(size.width(), size.height());
    return surface;
}

std::shared_ptr<RSurface> RSurface::WrapImage(std::shared_ptr<RImage> image) noexcept
{
    if (!image)
    {
        RLog(CZError, CZLN, "Invalid RImage");
        return {};
    }

    if (image->alphaType() == kUnpremul_SkAlphaType)
        RLog(CZWarning, CZLN, "The destination RImage is unpremultiplied alpha, but RSurface/Skia will still output premultiplied results");

    auto surface { std::shared_ptr<RSurface>(new RSurface(image)) };
    surface->m_self = surface;
    return surface;
}

std::shared_ptr<RPass> RSurface::beginPass(CZBitset<RPassCap> caps, RDevice *device) const noexcept
{
    return RPass::Make(caps, m_self.lock(), device);
}

bool RSurface::setGeometry(const RSurfaceGeometry &geometry) noexcept
{
    if (!geometry.isValid())
    {
        RLog(CZError, CZLN, "Failed to set RSurface geometry (invalid geometry)");
        return false;
    }

    m_geometry = geometry;
    return true;
}

bool RSurface::resize(SkISize size, SkScalar scale, bool shrink) noexcept
{
    SkISize imageSize (size.width() * scale, size.height() * scale);

    if (size.isEmpty() || imageSize.isEmpty())
    {
        RLog(CZWarning, CZLN, "Invalid dimensions ({},{}) or scale {}. Falling back to (1,1) and scale 1", size.width(), size.height(), scale);
        imageSize = size = {1, 1};
    }

    m_geometry.viewport.fRight = m_geometry.viewport.fLeft + size.width();
    m_geometry.viewport.fBottom = m_geometry.viewport.fTop + size.height();
    m_geometry.dst = SkRect::MakeWH(imageSize.width(), imageSize.height());

    if (shrink)
    {
        if (m_image->size() == imageSize)
            return false;
    }
    else if (m_image->size().width() >= imageSize.width() && m_image->size().height() >= imageSize.height())
        return false;

    RImageConstraints c {};
    c.allocator = m_image->allocator();
    c.readFormats = m_image->readFormats();
    c.writeFormats = m_image->writeFormats();
    c.caps[c.allocator] = m_image->checkDeviceCaps(RImageCap_All, c.allocator);

    auto image { RImage::Make(imageSize, { m_image->formatInfo().format, { m_image->modifier() } }, &c) };

    if (image)
        m_image = image;
    else
        RLog(CZError, CZLN, "Failed to allocate new RImage for RSurface. Keeping current RImage");

    return true;
}

RSurface::~RSurface() noexcept
{
    RResourceTrackerSub(RSurfaceRes);
}

RSurface::RSurface(std::shared_ptr<RImage> image) noexcept :
    m_image(image)
{
    assert(m_image);
    RResourceTrackerAdd(RSurfaceRes);
    setGeometry({
        .viewport = SkRect::Make(m_image->size()),
        .dst = SkRect::Make(m_image->size()),
        .transform = CZTransform::Normal});
}
