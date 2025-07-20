#ifndef RSURFACE_H
#define RSURFACE_H

#include <CZ/CZTransform.h>
#include <CZ/skia/core/SkRect.h>
#include <CZ/Ream/RObject.h>
#include <memory>

// Note: The alphaType() of the storage/destination image is ignored always considered premult alpha
class CZ::RSurface : public RObject
{
public:
    static std::shared_ptr<RSurface> WrapImage(std::shared_ptr<RImage> image, Int32 scale = 1) noexcept;

    RPass beginPass(RDevice *device = nullptr) const noexcept;
    RSKPass beginSKPass(RDevice *device = nullptr) const noexcept;

    std::shared_ptr<RImage> image() const noexcept { return m_image; }

    // Size (may differ from the image size if scale != 1 or the transform contains a 90° rotation)
    SkISize size() const noexcept { return m_size; }

    // Image scale factor
    Int32 scale() const noexcept { return m_scale; }

    // Position of the surface in the world
    SkIPoint pos() const noexcept {return m_pos; }

    // Transform
    CZTransform transform() const noexcept { return m_transform; }

    ~RSurface() noexcept;
private:
    RSurface(std::shared_ptr<RImage> image, Int32 scale) noexcept;;
    void calculateSizeFromImage() noexcept;
    std::shared_ptr<RImage> m_image;
    SkIPoint m_pos {};
    SkISize m_size {};
    Int32 m_scale { 1 };
    CZTransform m_transform { CZTransform::Normal };
    std::weak_ptr<RSurface> m_self;
};

#endif // RSURFACE_H
