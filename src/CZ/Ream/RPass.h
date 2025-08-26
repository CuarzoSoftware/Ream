#ifndef CZ_RPASS_H
#define CZ_RPASS_H

#include <CZ/Ream/RPainter.h>
#include <CZ/Ream/RPassCap.h>
#include <CZ/Ream/RSurfaceGeometry.h>
#include <CZ/CZBitset.h>

/**
 * @brief A Ream render pass.
 *
 * An RPass allows an RDevice to render into an RSurface using either RPainter or SkCanvas,
 * depending on the capabilities requested at creation time (see RSurface::beginPass()).
 *
 * If you mix both RPainter and SkCanvas commands, make sure to call either getPainter() or getCanvas()
 * first, depending on which API you intend to start using. Failing to do so may lead to synchronization issues,
 * or in some cases, may cause Skia to crash.
 *
 * Upon destruction, a fence is inserted into the underlying surface RImage to ensure that other threads wait
 * before using the image as a source. Destroying the pass may also trigger flushing of pending Skia commands,
 * depending on the current graphics backend.
 */
class CZ::RPass : public RObject
{
public:
    /**
     * @brief Destroys the render pass.
     *
     * Inserts a fence into the backing storage to ensure that subsequent use of the resulting RImage
     * from other threads is properly synchronized. May also flush pending Skia commands.
     */
    ~RPass() noexcept;

    /**
     * @brief Returns the capabilities enabled for this pass.
     *
     * Indicates whether RPainter and/or SkCanvas is available, based on the flags passed to RSurface::beginPass().
     *
     * @return A bitset of enabled RPassCap values.
     */
    CZBitset<RPassCap> caps() const noexcept { return m_caps; }

    /**
     * @brief Returns the SkCanvas associated with this pass, if available.
     *
     * @param sync Whether to synchronize and prepare the canvas for rendering.
     *             Pass false only if you intend to update canvas parameters without rendering.
     *
     * @return A pointer to the SkCanvas, or nullptr if SkCanvas is not supported by this pass.
     */
    virtual SkCanvas *getCanvas(bool sync = true) const noexcept = 0;

    /**
     * @brief Returns the RPainter associated with this pass, if available.
     *
     * @param sync Whether to synchronize and prepare the painter for rendering.
     *             Pass false only if you intend to update painter parameters without rendering.
     *
     * @return A pointer to the RPainter, or nullptr if RPainter is not supported by this pass.
     */
    virtual RPainter *getPainter(bool sync = true) const noexcept = 0;

    /**
     * @brief Sets transformation matrices.
     *
     * Updates both SkCanvas and RPainter transformation matrices to align with the given surface geometry.
     *
     * @param geometry The geometry to apply.
     */
    virtual void setGeometry(const RSurfaceGeometry &geometry) noexcept = 0;

    /**
     * @brief Resets the geometry to the default RSurface geometry.
     */
    void resetGeometry() noexcept;

    /**
     * @brief Returns the current geometry for this pass.
     *
     * Defaults to the geometry of the associated RSurface at creation time,
     * unless overridden via setGeometry().
     *
     * @return The current RSurfaceGeometry in use.
     */
    const RSurfaceGeometry &geometry() const noexcept { return m_geometry; }

    /**
     * @brief Returns the destination surface being rendered into.
     *
     * @return A shared pointer to the target RSurface.
     */
    std::shared_ptr<RSurface> surface() const noexcept { return m_surface; }

    /**
     * @brief Saves the state of both RPainter and SkCanvas.
     */
    void save() noexcept;

    /**
     * @brief Restores the most recently saved state of RPainter and SkCanvas.
     */
    void restore() noexcept;
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
