#include <CZ/Ream/VK/RVKPass.h>
#include <CZ/Ream/VK/RVKImage.h>
#include <CZ/Ream/VK/RVKDevice.h>
#include <CZ/Ream/VK/RVKPainter.h>
#include <CZ/Ream/RMatrixUtils.h>
#include <CZ/Ream/RSurface.h>
#include <CZ/Ream/RPainter.h>

#include <CZ/skia/core/SkCanvas.h>
#include <CZ/skia/core/SkSurface.h>
#include <CZ/skia/gpu/ganesh/GrDirectContext.h>

using namespace CZ;

RVKPass::RVKPass(std::shared_ptr<RSurface> surface, std::shared_ptr<RImage> image, std::shared_ptr<RPainter> painter,
                 sk_sp<SkSurface> skSurface, RDevice *device, CZBitset<RPassCap> caps) noexcept :
    RPass(surface, image, painter, skSurface, device, caps)
{
    // Reclaim this thread's finished deferred resources here (runs on the render thread each frame),
    // so command pools are always destroyed on the thread that created them (Vulkan external-sync
    // requirement; cross-thread destruction crashes the NVIDIA driver).
    if (auto *vk { device->asVK() })
        vk->clearGarbage();

    resetGeometry();
}

RVKPass::~RVKPass() noexcept
{
    // Submit any recorded painter work first (ordered before the base ~RPass write-sync submit).
    if (m_painter)
        static_cast<RVKPainter*>(m_painter.get())->flush();

    // Realize deferred Skia work on the queue so it is ordered before the write-sync submit
    // that the base ~RPass issues, then reconcile the image's tracked layout with Skia's.
    if (m_lastUsage == RPassCap_SkCanvas || m_skSurface)
    {
        if (auto *vk { m_device->asVK() })
            if (auto ctx { vk->skContext() })
                ctx->flushAndSubmit(GrSyncCpu::kNo);

        if (auto img { m_image->asVK() })
            img->syncLayoutFromSkia();
    }
}

SkCanvas *RVKPass::getCanvas(bool sync) const noexcept
{
    if (!m_skSurface)
        return nullptr;

    if (sync)
        m_lastUsage = RPassCap_SkCanvas;

    return m_skSurface->getCanvas();
}

RPainter *RVKPass::getPainter(bool sync) const noexcept
{
    if (!m_painter)
        return nullptr;

    if (sync)
        m_lastUsage = RPassCap_Painter;

    return m_painter.get();
}

void RVKPass::setGeometry(const RSurfaceGeometry &geometry) noexcept
{
    if (!geometry.isValid())
        return;

    m_geometry = geometry;

    if (m_painter)
        m_painter->setGeometry(geometry);

    if (m_skSurface)
        m_skSurface->getCanvas()->setMatrix(
            RMatrixUtils::VirtualToImage(geometry.transform, geometry.viewport, geometry.dst));
}
