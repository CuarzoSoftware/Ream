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
    resetGeometry();
}

RVKPass::~RVKPass() noexcept
{
    // Only the last one used has pending work (switching flushes the other one, see getCanvas() and
    // getPainter()), and only it knows the image's final layout.
    auto img { m_image->asVK() };

    if (m_lastUsage == RPassCap_SkCanvas)
    {
        // Realize deferred Skia work on the queue so it is ordered before the write-sync submit
        // that the base ~RPass issues, then take the layout Skia left the image in.
        if (auto *vk { m_device->asVK() })
            if (auto ctx { vk->skContext() })
                ctx->flushAndSubmit(GrSyncCpu::kNo);

        if (img)
            img->syncLayoutFromSkia();
    }

    // Submit any recorded painter work (ordered before the base ~RPass write-sync submit). Its
    // layout is the right one: told to Skia, never taken from it (Skia's record may be stale,
    // e.g. a pass with a Skia surface the painter alone used).
    if (m_painter)
    {
        static_cast<RVKPainter*>(m_painter.get())->flush();

        if (img && m_lastUsage != RPassCap_SkCanvas)
            img->syncLayoutToSkia();
    }
}

SkCanvas *RVKPass::getCanvas(bool sync) const noexcept
{
    RVKDevice::MarkSkiaUsed();
    if (!m_skSurface)
        return nullptr;

    if (sync && m_lastUsage != RPassCap_SkCanvas)
    {
        // The painters' work first (this pass' and any other recording: Skia's commands come
        // after them), and the layout changes told to Skia (else Skia draws or reads the image
        // from the layout it last knew: undefined, e.g. text missing)
        RVKPainter::FlushRecording();

        if (auto img { m_image->asVK() })
            img->syncLayoutToSkia();

        m_lastUsage = RPassCap_SkCanvas;
    }

    return m_skSurface->getCanvas();
}

RPainter *RVKPass::getPainter(bool sync) const noexcept
{
    if (!m_painter)
        return nullptr;

    if (sync && m_lastUsage != RPassCap_Painter)
    {
        // Skia's work first, and the layout it left the image in
        if (m_lastUsage == RPassCap_SkCanvas && m_skSurface)
        {
            if (auto *vk { m_device->asVK() })
                if (auto ctx { vk->skContext() })
                    ctx->flushAndSubmit(GrSyncCpu::kNo);

            if (auto img { m_image->asVK() })
                img->syncLayoutFromSkia();
        }

        m_lastUsage = RPassCap_Painter;
    }

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
