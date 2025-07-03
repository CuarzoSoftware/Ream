#ifndef RGLIMAGE_H
#define RGLIMAGE_H

#include <CZ/Ream/RImage.h>
#include <CZ/Ream/GL/RGLFormat.h>
#include <CZ/Ream/GL/RGLContext.h>
#include <EGL/egl.h>
#include <unordered_map>

namespace CZ
{
    struct RGLTexture
    {
        GLuint id;
        GLenum target;
    };

    struct RGLFramebufferInfo
    {
        GLuint id;
        SkISize size {};
        RFormat format;
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
    std::optional<GLuint> framebuffer(RGLDevice *device = nullptr) const noexcept;

    static std::shared_ptr<RGLImage> MakeFromPixels(const RPixelBufferInfo &params, RGLDevice *allocator = nullptr) noexcept;
    static std::shared_ptr<RGLImage> BorrowFramebuffer(const RGLFramebufferInfo &info, RGLDevice *allocator = nullptr) noexcept;

    bool writePixels(const RPixelBufferRegion &region) noexcept override;

    RGLDevice *allocator() const noexcept
    {
        return (RGLDevice*)m_allocator;
    }

private:
    struct GlobalDeviceData
    {
        RGLTexture texture {};
        CZOwnership textureOwnership { CZOwnership::Borrow };

        EGLImage image { EGL_NO_IMAGE };
        RGLDevice *device { nullptr };
    };

    class ThreadDeviceData : public RGLContextData
    {
    public:
        ThreadDeviceData(RGLDevice *device) noexcept :
            device(device) {};

        ~ThreadDeviceData() noexcept;

        GLuint rbo { 0 };
        CZOwnership rboOwnership { CZOwnership::Borrow };

        GLuint fb { 0 };
        CZOwnership fbOwnership { CZOwnership::Borrow };
        bool hasFb { false };

        RGLDevice *device { nullptr };
    };

    struct GlobalDeviceDataMap : public std::unordered_map<RGLDevice*, GlobalDeviceData>
    {
        ~GlobalDeviceDataMap() noexcept;
    };

    RGLImage(std::shared_ptr<RCore> core, RGLDevice *device, SkISize size, const RDRMFormat &format) noexcept
        : RImage(core, (RDevice*)device, size, format)
    {
        m_threadDataManager = RGLContextDataManager::Make([](RGLDevice *device) -> RGLContextData*
        {
            return new ThreadDeviceData(device);
        });
    }
    RGLFormat m_glFormat;
    GlobalDeviceDataMap m_devicesMap;
    std::shared_ptr<RGLContextDataManager> m_threadDataManager;
};

#endif // RGLIMAGE_H
