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

    [[nodiscard]] static std::shared_ptr<RGLImage> Make(SkISize size, const RDRMFormat &format, RStorageType storageType = RStorageType::Auto, RGLDevice *allocator = nullptr) noexcept;
    [[nodiscard]] static std::shared_ptr<RGLImage> BorrowFramebuffer(const RGLFramebufferInfo &info, RGLDevice *allocator = nullptr) noexcept;

    std::shared_ptr<RGBMBo> gbmBo(RDevice *device = nullptr) const noexcept override;
    std::shared_ptr<RDRMFramebuffer> drmFb(RDevice *device = nullptr) const noexcept override;
    bool writePixels(const RPixelBufferRegion &region) noexcept override;

    RGLDevice *allocator() const noexcept
    {
        return (RGLDevice*)m_allocator;
    }

private:

    [[nodiscard]] static std::shared_ptr<RGLImage> MakeWithGBMStorage(SkISize size, const RDRMFormat &format, RGLDevice *allocator = nullptr) noexcept;
    [[nodiscard]] static std::shared_ptr<RGLImage> MakeWithNativeStorage(SkISize size, const RDRMFormat &format, RGLDevice *allocator = nullptr) noexcept;

    bool writePixelsGBMMapWrite(const RPixelBufferRegion &region) noexcept;
    bool writePixelsNative(const RPixelBufferRegion &region) noexcept;
    void assignReadWriteFormats() noexcept;

    enum PF
    {
        PFStorageGBM        = 1 << 0,
        PFStorageNative     = 1 << 1
    };

    struct GlobalDeviceData
    {
        RGLTexture texture {};
        CZOwnership textureOwnership { CZOwnership::Borrow };

        GLuint rbo { 0 };
        CZOwnership rboOwnership { CZOwnership::Borrow };

        std::shared_ptr<REGLImage> eglImage;
        std::shared_ptr<RGBMBo> gbmBo;
        std::shared_ptr<RDRMFramebuffer> drmFb;
        RGLDevice *device { nullptr };
    };

    class ThreadDeviceData : public RGLContextData
    {
    public:
        ThreadDeviceData(RGLDevice *device) noexcept :
            device(device) {};

        ~ThreadDeviceData() noexcept;

        GLuint fb { 0 };
        CZOwnership fbOwnership { CZOwnership::Borrow };
        bool hasFb { false };

        RGLDevice *device { nullptr };
    };

    struct GlobalDeviceDataMap : public std::unordered_map<RGLDevice*, GlobalDeviceData>
    {
        ~GlobalDeviceDataMap() noexcept;
    };

    RGLImage(std::shared_ptr<RCore> core, RDevice *device, SkISize size, const RFormatInfo *formatInfo, SkAlphaType alphaType, const std::vector<RModifier> &modifiers) noexcept
        : RImage(core, (RDevice*)device, size, formatInfo, alphaType, modifiers)
    {
        m_threadDataManager = RGLContextDataManager::Make([](RGLDevice *device) -> RGLContextData*
        {
            return new ThreadDeviceData(device);
        });
    }

    CZBitset<PF> m_pf {};
    RGLFormat m_glFormat;
    mutable GlobalDeviceDataMap m_devicesMap;
    std::shared_ptr<RGLContextDataManager> m_threadDataManager;
};

#endif // RGLIMAGE_H
