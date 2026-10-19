#include <CZ/Ream/GL/RGLPass.h>
#include <CZ/Ream/GL/RGLImage.h>
#include <CZ/Ream/GL/RGLDevice.h>
#include <CZ/Ream/GL/RGLPainter.h>
#include <CZ/Ream/RMatrixUtils.h>
#include <CZ/skia/core/SkSurface.h>

using namespace CZ;

RGLPass::~RGLPass() noexcept
{
    if (m_lastUsage == 0)
        return; // Was not used at all

    if (m_lastUsage == RPassCap_SkCanvas)
    {
        m_skSurface->recordingContext()->asDirectContext()->flush(m_skSurface.get());

        /* Skia left the shared context's GL state in an undefined configuration for RGLPainter.
         * This pass ends here, but another pass (e.g. the one this was nested into, baking an
         * asset mid-render) may keep using its painter without a usage switch: its next draw
         * must re-sync or it can be silently discarded (stencil, color mask, etc). */
        RGLPainter::MarkSkiaGLStateDirty();
    }
}

SkCanvas *RGLPass::getCanvas(bool sync) const noexcept
{
    if (!m_caps.has(RPassCap_SkCanvas))
        return nullptr;

    if (sync)
        updateCurrent(RPassCap_SkCanvas);
    return m_skSurface->getCanvas();
}

RPainter *RGLPass::getPainter(bool sync) const noexcept
{
    if (!m_caps.has(RPassCap_Painter))
        return nullptr;

    if (sync)
        updateCurrent(RPassCap_Painter);
    return m_painter.get();
}

void RGLPass::setGeometry(const RSurfaceGeometry &geometry) noexcept
{
    if (!geometry.isValid())
        return;

    m_geometry = geometry;

    if (m_painter)
        m_painter->setGeometry(geometry);

    if (m_skSurface)
        m_skSurface->getCanvas()->setMatrix(RMatrixUtils::VirtualToImage(geometry.transform, geometry.viewport, geometry.dst));
}

RGLPass::RGLPass(std::shared_ptr<RSurface> surface, std::shared_ptr<RImage> image, std::shared_ptr<RPainter> painter, sk_sp<SkSurface> skSurface, RDevice *device, CZBitset<RPassCap> caps) noexcept :
    RPass(surface, image, painter, skSurface, device, caps)
{
    resetGeometry();
}

void RGLPass::updateCurrent(RPassCap cap) const noexcept
{
    auto *glDevice { m_device->asGL() };
    const EGLSurface eglSurface { m_image->asGL()->eglSurface(glDevice) };
    m_current.reset(new RGLMakeCurrent(glDevice->eglDisplay(), eglSurface, eglSurface, glDevice->eglContext()));

    if (m_lastUsage == cap)
        return;

    m_lastUsage = cap;

    if (cap == RPassCap_SkCanvas)
    {
        // Ensure RGLPainter commands are flushed
        glFlush();

        // Reset GL state modified by RGLPainter
        m_skSurface->recordingContext()->asDirectContext()->resetContext();

        // Skia is about to alter GL state RGLPainter relies on (see ~RGLPass)
        RGLPainter::MarkSkiaGLStateDirty();
    }
    else // RGLPainter
    {
        // Ensure SkCanvas commands are flushed
        if (m_skSurface)
            m_skSurface->recordingContext()->asDirectContext()->flush(m_skSurface.get());

        // Reset GL state modified by Skia
        RGLPainter::ResetGLState();
    }
}
