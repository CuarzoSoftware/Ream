#ifndef CZ_RGLPASS_H
#define CZ_RGLPASS_H

#include <CZ/Ream/RPass.h>
#include <CZ/Ream/GL/RGLMakeCurrent.h>

/**
 * @brief OpenGL backend implementation of RPass.
 *
 * Renders into an RSurface backed by an RGLImage using RGLPainter and/or Skia. It makes the
 * target image's EGL context/surface current on demand and, when switching between the RPainter
 * and SkCanvas APIs, flushes pending commands and resets the shared GL state so the two do not
 * interfere. Created via RSurface::beginPass().
 */
class CZ::RGLPass : public RPass
{
public:
    /**
     * @brief Destroys the pass, flushing any pending Skia commands if the SkCanvas API was last used.
     */
    ~RGLPass() noexcept;
    SkCanvas *getCanvas(bool sync = true) const noexcept override;
    RPainter *getPainter(bool sync = true) const noexcept override;
    void setGeometry(const RSurfaceGeometry &geometry) noexcept override;
private:
    friend class RPass;
    RGLPass(std::shared_ptr<RSurface> surface, std::shared_ptr<RImage> image, std::shared_ptr<RPainter> painter,
          sk_sp<SkSurface> skSurface, RDevice *device, CZBitset<RPassCap> caps) noexcept;
    void updateCurrent(RPassCap cap) const noexcept;
    mutable UInt32 m_lastUsage { 0 };
    mutable std::unique_ptr<RGLMakeCurrent> m_current;
};

#endif // CZ_RGLPASS_H
