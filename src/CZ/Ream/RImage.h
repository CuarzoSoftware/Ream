#ifndef RIMAGE_H
#define RIMAGE_H

#include <CZ/Ream/RObject.h>
#include <CZ/Ream/DRM/RDRMFormat.h>
#include <CZ/skia/core/SkSize.h>
#include <CZ/CZWeak.h>
#include <memory>

class CZ::RImage : public RObject
{
public:

    struct MakeFromPixelsParams
    {
        SkISize size;
        size_t stride;
        RFormat format;
        UInt8 *pixels;
    };

    static std::shared_ptr<RImage> MakeFromPixels(const MakeFromPixelsParams &params, RDevice *allocator = nullptr) noexcept;
    //static std::shared_ptr<RImage> MakeFromGBM(gbm_bo *bo, CZOwnership ownership) noexcept;

    SkISize size() const noexcept
    {
        return m_size;
    }

    const RDRMFormat &format() const noexcept
    {
        return m_format;
    }

    RDevice *allocator() const noexcept
    {
        return m_allocator;
    }

protected:
    RImage(std::shared_ptr<RCore> core, RDevice *device, SkISize size, const RDRMFormat &format) noexcept;
    SkISize m_size;
    RDRMFormat m_format;
    RDevice *m_allocator;
    std::shared_ptr<RCore> m_core;
};

#endif // RIMAGE_H
