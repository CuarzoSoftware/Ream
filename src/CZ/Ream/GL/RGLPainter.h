#ifndef RGLPAINTER_H
#define RGLPAINTER_H

#include <CZ/Ream/GL/RGLTexture.h>
#include <CZ/Ream/RPainter.h>
#include <CZ/Ream/GL/RGLShader.h>
#include <GLES2/gl2.h>

/**
 * @brief OpenGL backend implementation of RPainter.
 *
 * Executes the painter's draw operations using hand-written GLSL programs (RGLProgram/RGLShader)
 * selected by feature flags, rendering into the framebuffer of the pass's surface image. Created
 * internally by RGLDevice; obtained from a pass via RPass::getPainter().
 */
class CZ::RGLPainter final : public RPainter
{
public:
    bool drawImage(const RDrawImageInfo& image, const SkRegion *region = nullptr, const RDrawImageInfo* mask = nullptr) noexcept override;
    bool drawImageEffect(const RDrawImageInfo& image, ImageEffect effect, const SkRegion *region = nullptr) noexcept override;
    bool drawColor(const SkRegion &region) noexcept override;

    /**
     * @brief Returns the device used for rendering by this painter as an RGLDevice.
     */
    RGLDevice *device() const noexcept { return (RGLDevice*)m_device; }
    bool setGeometry(const RSurfaceGeometry &geometry) noexcept override;

    /**
     * @brief Marks the current thread's GL state as dirtied by Skia.
     *
     * Skia (SkCanvas) draws leave GL state the painter doesn't rebind on every draw (stencil,
     * color mask, etc.) in an undefined configuration. Passes call this whenever a canvas is
     * used so the next painter draw on this thread re-syncs the state first — including draws
     * from a DIFFERENT pass (e.g. an asset baked with Skia in the middle of another pass's
     * painter rendering, which would otherwise silently discard the following draws).
     */
    static void MarkSkiaGLStateDirty() noexcept;

    /**
     * @brief Unconditionally resets the GL state Skia may have altered to what RGLPainter expects.
     *
     * Must be called with the target EGL context current.
     */
    static void ResetGLState() noexcept;

private:
    /// Calls ResetGLState() if MarkSkiaGLStateDirty() was called since the last sync (per thread)
    static void SyncSkiaGLState() noexcept;

    CZBitset<RGLShader::Features> calcDrawImageFeatures(std::shared_ptr<RImage> image, RGLTexture *imageTex, RGLTexture *maskTex) const noexcept;
    CZBitset<RGLShader::Features> calcDrawColorFeatures(SkScalar finalAlpha) const noexcept;
    SkColor4f calcDrawColorColor() const noexcept;
    void setDrawColorUniforms(CZBitset<RGLShader::Features> features, std::shared_ptr<RGLProgram> prog, const SkColor4f &colorF) const noexcept;
    void setDrawColorBlendFunc(CZBitset<RGLShader::Features> features) const noexcept;

    std::vector<GLfloat> genVBO(const SkRegion &region) const noexcept;
    SkRegion calcDrawImageRegion(RSurface *surface, const RDrawImageInfo &imageInfo, const SkRegion *clip, const RDrawImageInfo *maskInfo) const noexcept;
    void calcPosProj(RSurface *surface, bool flipY, SkScalar *outMat) const noexcept;
    void calcImageProj(const RDrawImageInfo &info, SkScalar *outMat) const noexcept;
    void setScissors(RSurface *surface, bool flipY, const SkRegion &region) const noexcept;
    void bindTexture(RGLTexture tex, GLuint uniform, const RDrawImageInfo &info, GLuint slot) const noexcept;
    friend class RGLDevice;
    friend class RGLShader;
    friend class RGLProgram;
    RGLPainter(std::shared_ptr<RSurface> surface, RGLDevice *device) noexcept : RPainter(surface, (RDevice*)device) {};
};

#endif // RGLPAINTER_H
