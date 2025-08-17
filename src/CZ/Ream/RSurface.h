#ifndef RSURFACE_H
#define RSURFACE_H

#include <CZ/CZTransform.h>
#include <CZ/Ream/RObject.h>
#include <CZ/skia/core/SkRect.h>
#include <memory>

// Note: The alphaType() of the storage/destination image is ignored and always considered premult alpha
class CZ::RSurface : public RObject
{
public:
    static std::shared_ptr<RSurface> WrapImage(std::shared_ptr<RImage> image) noexcept;

    RPass beginPass(RDevice *device = nullptr) const noexcept;
    RSKPass beginSKPass(RDevice *device = nullptr) const noexcept;

    std::shared_ptr<RImage> image() const noexcept { return m_image; }

    // Transform applied to the surface
    CZTransform transform() const noexcept { return m_transform; }

    // Portion of the world being captured (virtual coords)
    SkRect viewport() const noexcept { return m_viewport; }

    // Destination within the surface the viewport is rendered into (pixel coords)
    SkRect dst() const noexcept { return m_dst; }

    bool setGeometry(SkRect viewport, SkRect dst, CZTransform transform) noexcept;
    ~RSurface() noexcept;
private:
    RSurface(std::shared_ptr<RImage> image) noexcept;
    std::shared_ptr<RImage> m_image;
    SkRect m_viewport {};
    SkRect m_dst {};
    CZTransform m_transform { CZTransform::Normal };
    std::weak_ptr<RSurface> m_self;
};

#endif // RSURFACE_H
