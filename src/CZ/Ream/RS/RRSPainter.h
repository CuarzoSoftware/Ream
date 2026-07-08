#ifndef CZ_RRSPAINTER_H
#define CZ_RRSPAINTER_H

#include <CZ/Ream/RPainter.h>

/**
 * @brief Raster (software) implementation of RPainter.
 *
 * RRSPainter carries out RPainter's drawing operations on the CPU by rendering into the target
 * surface's SkSurface with Skia (shaders, color filters, blend modes and clip regions), rather than
 * through a GPU pipeline. It is created by RRSDevice and drives an RRSImage-backed surface.
 */
class CZ::RRSPainter final : public RPainter
{
public:
    /**
     * @brief Draws an image (optionally masked) into the surface. Implements RPainter::drawImage().
     *
     * The source image is drawn through a Skia shader built from the draw info (source
     * rect/transform, filtering and wrap modes), honoring the current opacity, blend mode,
     * per-channel factor and optional color replacement. When a mask is provided it is applied as
     * a DstIn alpha mask.
     */
    bool drawImage(const RDrawImageInfo &image, const SkRegion *region = nullptr, const RDrawImageInfo *mask = nullptr) noexcept override;

    /**
     * @brief Draws an image effect into the surface. Implements RPainter::drawImageEffect().
     *
     * @note The Raster backend does not implement the actual effects; it currently fills the
     *       affected region with a fixed translucent color as a placeholder.
     */
    bool drawImageEffect(const RDrawImageInfo& image, ImageEffect effect, const SkRegion *region = nullptr) noexcept override;

    /**
     * @brief Fills the given region with the current color. Implements RPainter::drawColor().
     *
     * The region is intersected with the current viewport, and the fill honors the current opacity,
     * blend mode, per-channel factor and premultiplied-color option.
     */
    bool drawColor(const SkRegion &region) noexcept override;

    /**
     * @brief Returns the associated device downcast to the Raster backend type.
     */
    RRSDevice *device() const noexcept { return (RRSDevice*)m_device; }

    /**
     * @brief Sets the surface geometry used to build the drawing matrix. Implements RPainter::setGeometry().
     *
     * @return false if the geometry is invalid, true otherwise.
     */
    bool setGeometry(const RSurfaceGeometry &geometry) noexcept override;
private:
    friend class RRSDevice;

    enum class ValRes
    {
        Ok,
        Noop,
        Error
    };

    RRSPainter(std::shared_ptr<RSurface> surface, RRSDevice *device) noexcept : RPainter(surface, (RDevice*)device) {};
    ValRes validateDrawImage(const RDrawImageInfo &image, const SkRegion *region, const RDrawImageInfo *mask, std::shared_ptr<RSurface> surface, SkPath &outClip) noexcept;
};

#endif // CZ_RRSPAINTER_H
