#include <CZ/Ream/RSurface.h>
#include <CZ/Ream/RCore.h>
#include <CZ/Ream/RDevice.h>
#include <CZ/Ream/RPainter.h>
#include <CZ/Ream/RImage.h>

using namespace CZ;

std::shared_ptr<RSurface> RSurface::WrapImage(std::shared_ptr<RImage> image, Int32 scale) noexcept
{
    if (!image) return {};
    if (scale <= 0) scale = 1;

    auto surface { std::shared_ptr<RSurface>(new RSurface(image, scale)) };
    surface->m_self = surface;
    surface->calculateSizeFromImage();
    return surface;
}

RPainter *RSurface::painter(RDevice *device) const noexcept
{
    if (!device)
        device = RCore::Get()->mainDevice();

    device->painter()->m_surface = m_self.lock();
    return device->painter();
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
