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
    {
        // Was not used at all
        return;
    }

    // Force flush
    const auto currentUsage { m_lastUsage };
    m_lastUsage = 0;
    updateCurrent((RPassCap)currentUsage);
}

SkCanvas *RGLPass::getCanvas() const noexcept
{
    if (!m_caps.has(RPassCap_SkCanvas))
        return nullptr;

    updateCurrent(RPassCap_SkCanvas);
    return m_skSurface->getCanvas();
}

RPainter *RGLPass::getPainter() const noexcept
{
    if (!m_caps.has(RPassCap_Painter))
        return nullptr;

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
    }
    else // RGLPainter
    {
        // Ensure SkCanvas commands are flushed
        if (m_skSurface)
            m_skSurface->recordingContext()->asDirectContext()->flush(m_skSurface.get());

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1.0f);
        glEnable(GL_BLEND);
        glEnable(GL_TEXTURE_2D);
        glEnable(GL_SCISSOR_TEST);
        glDisable(GL_STENCIL_TEST);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glDisable(GL_DITHER);
        glDisable(GL_POLYGON_OFFSET_FILL);
        glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
        glDisable(GL_SAMPLE_COVERAGE);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glCullFace(GL_BACK);
        glLineWidth(1);
        glHint(GL_GENERATE_MIPMAP_HINT, GL_FASTEST);
        glPolygonOffset(0, 0);
        glDepthFunc(GL_LESS);
        glDepthRangef(0, 1);
        glStencilMask(1);
        glDepthMask(GL_FALSE);
        glFrontFace(GL_CCW);
        glBlendColor(0, 0, 0, 0);
        glBlendEquation(GL_FUNC_ADD);
    }
}
