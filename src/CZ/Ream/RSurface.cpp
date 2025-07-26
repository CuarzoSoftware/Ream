#include <CZ/skia/gpu/ganesh/GrDirectContext.h>
#include <CZ/skia/gpu/ganesh/GrRecordingContext.h>
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

std::shared_ptr<RSurface> RSurface::WrapImage(std::shared_ptr<RImage> image, Int32 scale) noexcept
{
    if (!image)
    {
        RLog(CZError, CZLN, "Invalid RImage");
        return {};
    }

    if (scale <= 0)
    {
        RLog(CZWarning, CZLN, "Invalid scale factor <= 0. Replacing it with 1...");
        scale = 1;
    }

    if (image->alphaType() == kUnpremul_SkAlphaType)
        RLog(CZWarning, CZLN, "The destination RImage is unpremultiplied alpha, but RSurface/Skia will still output premultiplied results");

    auto surface { std::shared_ptr<RSurface>(new RSurface(image, scale)) };
    surface->m_self = surface;
    surface->calculateSizeFromImage();
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

    return { m_image, device };
}

RSurface::~RSurface() noexcept
{
    RResourceTrackerSub(RSurfaceRes);
}

RSurface::RSurface(std::shared_ptr<RImage> image, Int32 scale) noexcept :
    m_image(image), m_scale(scale)
{
    RResourceTrackerAdd(RSurfaceRes);
}

void RSurface::calculateSizeFromImage() noexcept
{
    if (!m_image) return;

    m_size.set(
        image()->size().width() / scale(),
        image()->size().height() / scale());

    if (CZ::Is90Transform(transform()))
    {
        const Int32 w { size().width() };
        m_size.fWidth = size().height();
        m_size.fHeight = w;
    }
}
