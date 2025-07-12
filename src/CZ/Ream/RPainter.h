#ifndef RPAINTER_H
#define RPAINTER_H

#include <CZ/skia/core/SkRRect.h>
#include <CZ/Ream/RObject.h>
#include <CZ/CZTransform.h>
#include <memory>

#include <CZ/skia/core/SkBlendMode.h>

class CZ::RPainter : public RObject
{
public:    
    virtual bool drawImage(const SkRegion &region) noexcept = 0;

    void setImage(std::shared_ptr<RImage> image) noexcept { m_image = image; };
    std::shared_ptr<RImage> image() const noexcept { return m_image.lock(); };

    void setImageScale(Float32 scale) noexcept
    {
        if (scale <= 0.125f) scale = 0.125f;
        m_imageScale = scale;
    }
    Float32 imageScale() const noexcept { return m_imageScale; }

    void setImageSrcTransform(CZTransform transform) noexcept { m_imageSrcTransform = transform; }
    CZTransform imageSrcTransform() const noexcept { return m_imageSrcTransform; };

    void setImageSrcRect(const SkRect &rect) noexcept { m_imageSrcRect = rect; }
    const SkRect &imageSrcRect() const noexcept { return m_imageSrcRect; }

    void setImageDstRect(const SkIRect &rect) noexcept { m_imageDstRect = rect; }
    const SkIRect &imageDstRect() const noexcept { return m_imageDstRect; }

    RDevice *device() const noexcept { return m_device; }
protected:
    friend class RSurface;
    RPainter(RDevice *device) noexcept : m_device(device) {}
    std::weak_ptr<RSurface> m_surface;
    std::weak_ptr<RImage> m_image;
    SkRect m_imageSrcRect {};
    Float32 m_imageScale { 1.f };
    CZTransform m_imageSrcTransform { CZTransform::Normal };
    SkIRect m_imageDstRect {};
    RDevice *m_device;
};

#endif // RPAINTER_H
