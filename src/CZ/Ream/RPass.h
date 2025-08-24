#ifndef CZ_RPASS_H
#define CZ_RPASS_H

#include <CZ/Ream/RPainter.h>
#include <CZ/Ream/RPassCap.h>
#include <CZ/Ream/RSurfaceGeometry.h>
#include <CZ/CZBitset.h>

/**
 * @brief A Ream render pass.
 *
 * An RPass allows an RDevice to render into an RSurface using both RPainter or SkCanvas depending on the requested capabilities at creation time (see RSurface::beginPass()).
 *
 * If you mix both RPainter and SkCanvas commands make sure to always call getCanvas() or getPainter() first dependening on which one you start using.
 * Not doing so may lead to sync issues or even a segfault depending on the current graphics backend.
 *
 * At destruction time a fence is added to the backign storage RImage to ensure other threads wait before using the image as source.
 * Destroying the pass may also trigger flushing of Skia commands depending on the current grapgic backend.
 */
class CZ::RPass : public RObject
{
public:
    ~RPass() noexcept;

    CZBitset<RPassCap> caps() const noexcept { return m_caps; }

    // Can be nullptr depending on the caps
    virtual SkCanvas *getCanvas() const noexcept = 0;

    // Can be nullptr depending on the caps
    virtual RPainter *getPainter() const noexcept = 0;

    // Updates RPainter and SkCanvas matrices to match the given geometry
    virtual void setGeometry(const RSurfaceGeometry &geometry) noexcept = 0;

    // Restores the geometry to RSurface::geometry()
    void resetGeometry() noexcept;

    const RSurfaceGeometry &geometry() const noexcept { return m_geometry; }

    // The desination surface
    std::shared_ptr<RSurface> surface() const noexcept { return m_surface; }
protected:
    friend class RSurface;
    static std::shared_ptr<RPass> Make(CZBitset<RPassCap> caps, std::shared_ptr<RSurface> surface, RDevice *device) noexcept;
    RPass(std::shared_ptr<RSurface> surface, std::shared_ptr<RImage> image, std::shared_ptr<RPainter> painter,
          sk_sp<SkSurface> skSurface, RDevice *device, CZBitset<RPassCap> caps) noexcept;
    RSurfaceGeometry m_geometry {};
    std::shared_ptr<RSurface> m_surface;
    std::shared_ptr<RImage> m_image;
    std::shared_ptr<RPainter> m_painter;
    sk_sp<SkSurface> m_skSurface;
    RDevice *m_device;
    CZBitset<RPassCap> m_caps;
};

#endif // CZ_RPASS_H
