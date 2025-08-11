#ifndef RGLIMAGE_H
#define RGLIMAGE_H

#include <CZ/CZBitset.h>
#include <CZ/Ream/GL/RGLTexture.h>
#include <CZ/Ream/RImage.h>
#include <CZ/Ream/GL/RGLFormat.h>
#include <CZ/Ream/GL/RGLContext.h>
#include <EGL/egl.h>
#include <unordered_map>

namespace CZ
{
    struct RGLFramebufferInfo
    {
        GLuint id;
        SkISize size {};
        RFormat format;
        SkAlphaType alphaType { kUnknown_SkAlphaType };
    };
}

class CZ::RGLImage : public RImage
{
public:
    /**
     * @brief Returns the OpenGL texture id and target for the current device.
     *
     * @returns {0, 0} if no device is bound or the image couldn't be imported.
     */
    RGLTexture texture(RGLDevice *device = nullptr) const noexcept;
    std::optional<GLuint> glFb(RGLDevice *device = nullptr) const noexcept;
    std::shared_ptr<REGLImage> eglImage(RGLDevice *device = nullptr) const noexcept;

    [[nodiscard]] static std::shared_ptr<RGLImage> Make(SkISize size, const RDRMFormat &format, const RImageConstraints *constraints = nullptr) noexcept;
    [[nodiscard]] static std::shared_ptr<RGLImage> BorrowFramebuffer(const RGLFramebufferInfo &info, RGLDevice *allocator = nullptr) noexcept;
    [[nodiscard]] static std::shared_ptr<RGLImage> FromDMA(const RDMABufferInfo &info, CZOwn ownership, const RImageConstraints *constraints = nullptr) noexcept;

    std::shared_ptr<RGBMBo> gbmBo(RDevice *device = nullptr) const noexcept override;
    std::shared_ptr<RDRMFramebuffer> drmFb(RDevice *device = nullptr) const noexcept override;
    sk_sp<SkImage> skImage(RDevice *device = nullptr) const noexcept override;
    sk_sp<SkSurface> skSurface(RDevice *device = nullptr) const noexcept override;
    CZBitset<RImageCap> checkDeviceCaps(CZBitset<RImageCap> caps, RDevice *device = nullptr) const noexcept override;

    bool writePixels(const RPixelBufferRegion &region) noexcept override;
    bool readPixels(const RPixelBufferRegion &region) noexcept override;

    RGLDevice *allocator() const noexcept
    {
        return (RGLDevice*)m_allocator;
    }

private:

    [[nodiscard]] static std::shared_ptr<RGLImage> MakeWithGBMStorage(SkISize size, const RDRMFormat &format, const RImageConstraints *constraints) noexcept;
    [[nodiscard]] static std::shared_ptr<RGLImage> MakeWithNativeStorage(SkISize size, const RDRMFormat &format, const RImageConstraints *constraints) noexcept;

    bool writePixelsGBMMapWrite(const RPixelBufferRegion &region) noexcept;
    bool writePixelsNative(const RPixelBufferRegion &region) noexcept;
    bool readPixelsGBMmapRead(const RPixelBufferRegion &region) noexcept;
    bool readPixelsNative(const RPixelBufferRegion &region) noexcept;

    void assignReadWriteFormats() noexcept;

    /* If set, re-checks can be omitted */
    enum UnsupportedDeviceCap
    {
        NoGLTexture     = 1 << 0,
        NoGLFramebufer  = 1 << 1,
        NoEGLImage      = 1 << 2,
        NoGBMBo         = 1 << 3,
        NoDRMFb         = 1 << 4,
        NoSkImage       = 1 << 5,
        NoSkSurface     = 1 << 6,
    };

    /* Private flags */
    enum PF
    {
        PFStorageGBM        = 1 << 0,
        PFStorageNative     = 1 << 1
    };

    /* Device data shared across GL contexts */
    struct GlobalDeviceData
    {
        CZBitset<UnsupportedDeviceCap> unsupportedCaps;
        RGLTexture texture {};
        CZOwn textureOwnership { CZOwn::Borrow };

        std::shared_ptr<REGLImage> eglImage;
        std::shared_ptr<RGBMBo> gbmBo;
        std::shared_ptr<RDRMFramebuffer> drmFb;
        RGLDevice *device { nullptr };
    };

    struct GlobalDeviceDataMap : public std::unordered_map<RGLDevice*, GlobalDeviceData>
    {
        ~GlobalDeviceDataMap() noexcept;
    };

    /* Device data for a specific GL context */
    class ContextData : public RGLContextData
    {
    public:
        ContextData(RGLDevice *device) noexcept : device(device) {};
        ~ContextData() noexcept;

        sk_sp<SkImage> skImage;
        sk_sp<SkSurface> skSurface;
        std::optional<GLuint> glFb {};
        CZOwn fbOwnership { CZOwn::Borrow };
        RGLDevice *device { nullptr };
    };

    RGLImage(std::shared_ptr<RCore> core, RDevice *device, SkISize size, const RFormatInfo *formatInfo, SkAlphaType alphaType, RModifier modifier) noexcept;

    CZBitset<PF> m_pf {};
    RGLFormat m_glFormat;
    mutable GlobalDeviceDataMap m_devicesMap;
    std::shared_ptr<RGLContextDataManager> m_contextDataManager;
};

#endif // RGLIMAGE_H
