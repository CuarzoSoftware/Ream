#ifndef RSURFACE_H
#define RSURFACE_H

#include <CZ/CZTransform.h>
#include <CZ/CZBitset.h>
#include <CZ/Ream/RObject.h>
#include <CZ/Ream/RSurfaceGeometry.h>
#include <CZ/Ream/RPassCap.h>
#include <CZ/skia/core/SkRect.h>
#include <memory>

namespace CZ { struct RImageConstraints; }

/**
 * @brief Abstraction for rendering into an RImage.
 *
 * RSurface wraps an RImage and provides convenience APIs to render using either RPainter or Skia SkCanvas.
 * It maintains a virtual viewport and pixel destination rectangle to control coordinate mapping.
 */
class CZ::RSurface : public RObject
{
public:

    /**
     * @brief Creates a new surface with an internal image that supports all major capabilities.
     *
     * The created image will have at least the following caps for the main device:
     * - RImageCap_Src
     * - RImageCap_Dst
     * - RImageCap_SkImage
     * - RImageCap_SkSurface
     *
     * @param size Viewport size (unscaled).
     * @param scale Scale factor.
     * @param alpha Whether the RImage format has alpha.
     * @return A shared pointer to the new RSurface, or nullptr on failure.
     *
     * @note If additional caps are needed, create an RImage manually and use WrapImage().
     */
    [[nodiscard]] static std::shared_ptr<RSurface> Make(SkISize size, SkScalar scale, bool alpha) noexcept;

    /**
     * @brief Creates a surface from an existing RImage.
     *
     * Wraps an existing image without copying it.
     *
     * @note The alphaType() of the backing storage/destination image is ignored and always considered premultiplied alpha.
     *
     * @param image An existing image with suitable caps.
     * @return A new surface wrapping the image, or nullptr on failure.
     */
    [[nodiscard]] static std::shared_ptr<RSurface> WrapImage(std::shared_ptr<RImage> image) noexcept;

    /**
     * @brief Begins a rendering pass using an RPainter.
     *
     * @param device Optional device to use. If nullptr, the main device is used.
     * @return A valid RPass object on success, or an invalid RPass on failure.
     */
    [[nodiscard]] std::shared_ptr<RPass> beginPass(CZBitset<RPassCap> caps = RPassCap_Painter | RPassCap_SkCanvas, RDevice *device = nullptr) const noexcept;

    /**
     * @brief Returns the backing image used by the surface.
     *
     * @return Shared pointer to the underlying RImage. Always valid.
     */
    std::shared_ptr<RImage> image() const noexcept { return m_image; }

    /**
     * @brief Updates the viewport -> (transform) -> dst mapping.
     *
     * Unlike resize(), it never allocates a new image, even if the destination exceeds image bounds.
     *
     * @param viewport Portion of the world to capture.
     * @param dst Pixel area within the image to render into.
     * @param transform Optional transform to apply (defaults to CZTransform::Normal).
     * @return True if geometry was successfully set; false otherwise.
     */
    bool setGeometry(const RSurfaceGeometry &geometry) noexcept;
    const RSurfaceGeometry &geometry() const noexcept { return m_geometry; }

    /**
     * @brief Resizes the surface and optionally shrinks the backing image to fit the new destination size.
     *
     * @param size Unscaled image size (viewport size).
     * @param scale Scale factor.
     * @param shrink If true, a new image will be allocated if the new dst size is different than the current image.
     *               If false, the existing image is reused as long as it is large enough.
     * @return True if a new image was allocated, false if the current image was reused.
     */
    bool resize(SkISize size, SkScalar scale, bool shrink = false) noexcept;

    /**
     * @brief Destructor.
     */
    ~RSurface() noexcept;
private:
    RSurface(std::shared_ptr<RImage> image) noexcept;
    std::shared_ptr<RImage> m_image;
    RSurfaceGeometry m_geometry {};
    std::weak_ptr<RSurface> m_self;
};

#endif // RSURFACE_H
