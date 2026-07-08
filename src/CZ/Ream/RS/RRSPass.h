#ifndef CZ_RRSPASS_H
#define CZ_RRSPASS_H

#include <CZ/Ream/RPass.h>
#include <CZ/Ream/GL/RGLMakeCurrent.h>

/**
 * @brief Raster (software) implementation of RPass.
 *
 * RRSPass is the render pass used by the CPU software backend. Since Raster surfaces are backed by
 * an SkSurface, both the RPainter and SkCanvas returned here operate directly on that surface, and
 * no GPU synchronization is required (the @c sync arguments are ignored).
 */
class CZ::RRSPass : public RPass
{
public:
    /**
     * @brief Destroys the render pass.
     */
    ~RRSPass() noexcept;

    /**
     * @brief Returns the surface's SkCanvas. Implements RPass::getCanvas().
     *
     * @note The @p sync argument is ignored by the Raster backend.
     * @return The canvas, or nullptr if the pass has no SkSurface.
     */
    SkCanvas *getCanvas(bool sync = true) const noexcept override;

    /**
     * @brief Returns the pass's RPainter. Implements RPass::getPainter().
     *
     * @note The @p sync argument is ignored by the Raster backend.
     */
    RPainter *getPainter(bool sync = true) const noexcept override;

    /**
     * @brief Applies the geometry to both the RPainter and the SkCanvas matrix. Implements RPass::setGeometry().
     */
    void setGeometry(const RSurfaceGeometry &geometry) noexcept override;
protected:
    friend class RPass;
    RRSPass(std::shared_ptr<RSurface> surface, std::shared_ptr<RImage> image, std::shared_ptr<RPainter> painter,
            sk_sp<SkSurface> skSurface, RDevice *device, CZBitset<RPassCap> caps) noexcept;
};
#endif // CZ_RRSPASS_H
