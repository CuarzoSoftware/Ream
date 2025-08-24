#include <CZ/Ream/RPass.h>
#include <CZ/Ream/RSurface.h>
#include <CZ/Ream/RImage.h>
#include <CZ/Ream/RSync.h>
#include <CZ/Ream/RDevice.h>
#include <CZ/Ream/RCore.h>

#include <CZ/Ream/GL/RGLPass.h>
#include <CZ/Ream/RS/RRSPass.h>

using namespace CZ;

RPass::RPass(std::shared_ptr<RSurface> surface, std::shared_ptr<RImage> image, std::shared_ptr<RPainter> painter, sk_sp<SkSurface> skSurface, RDevice *device, CZBitset<RPassCap> caps) noexcept :
    m_surface(surface), m_image(image), m_painter(painter), m_skSurface(skSurface), m_device(device), m_caps(caps)
{
    if (m_image->readSync())
        m_image->readSync()->gpuWait(device);

    if (m_image->writeSync())
        m_image->writeSync()->gpuWait(device);
}

RPass::~RPass() noexcept
{
   m_image->setWriteSync(RSync::Make(m_device));
}

void RPass::resetGeometry() noexcept
{
    setGeometry(surface()->geometry());
}

std::shared_ptr<RPass> RPass::Make(CZBitset<RPassCap> caps, std::shared_ptr<RSurface> surface, RDevice *device) noexcept
{
    auto core { RCore::Get() };

    if (caps.isEmpty())
    {
        RLog(CZError, CZLN, "Failed to RPass (invalid caps = 0)");
        return {};
    }

    if (!device)
        device = RCore::Get()->mainDevice();

    std::shared_ptr<RPainter> painter;

    if (caps.has(RPassCap_Painter))
    {
        if (!surface->image()->checkDeviceCaps(RImageCap_Dst, device))
        {
            device->log(CZError, CZLN, "Failed to create RPass (RImageCap_Dst not satisfied)");
            return {};
        }

        painter = device->makePainter(surface);

        if (!painter)
        {
            device->log(CZError, CZLN, "Failed to create RPass (missing RPainter)");
            return {};
        }
    }

    sk_sp<SkSurface> skSurface;

    if (caps.has(RPassCap_SkCanvas))
    {
        if (!surface->image()->checkDeviceCaps(RImageCap_SkSurface, device))
        {
            RLog(CZError, CZLN, "Failed to create RPass (RImageCap_SkSurface not satisfied)");
            return {};
        }

        skSurface = surface->image()->skSurface(device);
        assert(skSurface);
    }

    if (device->asGL())
        return std::shared_ptr<RGLPass>(new RGLPass(surface, surface->image(), painter, skSurface, device, caps));
    else if (device->asRS())
        return std::shared_ptr<RRSPass>(new RRSPass(surface, surface->image(), painter, skSurface, device, caps));

    return nullptr; // TODO: RVKPass
}
