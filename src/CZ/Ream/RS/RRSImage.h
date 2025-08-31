#ifndef CZ_RRSIMAGE_H
#define CZ_RRSIMAGE_H

#include <CZ/Ream/RImage.h>
#include <CZ/Core/CZSharedMemory.h>
#include <CZ/Core/CZBitset.h>
#include <EGL/egl.h>

class CZ::RRSImage : public RImage
{
public:
    [[nodiscard]] static std::shared_ptr<RRSImage> Make(SkISize size, const RDRMFormat &format, const RImageConstraints *constraints = nullptr) noexcept;

    std::shared_ptr<RGBMBo> gbmBo(RDevice *device = nullptr) const noexcept override
    {
        CZ_UNUSED(device)
        return {};
    }

    std::shared_ptr<RDRMFramebuffer> drmFb(RDevice *device = nullptr) const noexcept override
    {
        CZ_UNUSED(device)
        return {};
    }

    sk_sp<SkImage> skImage(RDevice *device = nullptr) const noexcept override
    {
        CZ_UNUSED(device)
        return m_skImage;
    }

    sk_sp<SkSurface> skSurface(RDevice *device = nullptr) const noexcept override
    {
        CZ_UNUSED(device)
        return m_skSurface;
    }

    CZBitset<RImageCap> checkDeviceCaps(CZBitset<RImageCap> caps, RDevice *device = nullptr) const noexcept override;
    bool writePixels(const RPixelBufferRegion &region) noexcept override;
    bool readPixels(const RPixelBufferRegion &region) noexcept override;
    RRSDevice *allocator() const noexcept { return (RRSDevice*)m_allocator; }
    size_t stride() const noexcept { return m_stride; }
private:
    RRSImage(std::shared_ptr<RCore> core, std::shared_ptr<CZSharedMemory> shm, sk_sp<SkImage> skImage, sk_sp<SkSurface> skSurface,
             RDevice *device, SkISize size, size_t stride, const RFormatInfo *formatInfo, SkAlphaType alphaType, RModifier modifier) noexcept;
    size_t m_stride;
    std::shared_ptr<CZSharedMemory> m_shm;
    sk_sp<SkImage> m_skImage;
    sk_sp<SkSurface> m_skSurface;
};
#endif // CZ_RRSIMAGE_H
