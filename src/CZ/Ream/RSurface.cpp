#include <CZ/skia/gpu/ganesh/GrDirectContext.h>
#include <CZ/skia/gpu/ganesh/GrRecordingContext.h>
#include <CZ/Ream/RMatrixUtils.h>
#include <CZ/Ream/SK/RSKPass.h>
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

RPass RSurface::beginPass(RDevice *device) const noexcept
{
    if (!device)
        device = RCore::Get()->mainDevice();

    auto *painter { device->painter() };

    if (!painter)
        return {};

    return { m_self.lock(), painter };
}

RSKPass RSurface::beginSKPass(RDevice *device) const noexcept
{
    if (!device)
        device = RCore::Get()->mainDevice();

    return { RMatrixUtils::VirtualToImage(transform(), viewport(), dst()), m_image, device };
}

bool RSurface::setGeometry(SkRect viewport, SkRect dst, CZTransform transform) noexcept
{
    if (!viewport.isSorted() || !dst.isSorted() ||
        viewport.isEmpty() || dst.isEmpty() ||
        !viewport.isFinite() || !dst.isFinite())
        return false;

    m_viewport = viewport;
    m_dst = dst;
    m_transform = transform;
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
    setGeometry(SkRect::Make(m_image->size()), SkRect::Make(m_image->size()), CZTransform::Normal);
}
