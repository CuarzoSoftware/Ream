#ifndef RGLIMAGE_H
#define RGLIMAGE_H

#include <CZ/Ream/RImage.h>
#include <CZ/Ream/GL/RGLFormat.h>
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
    struct DeviceData
    {
        RGLTexture texture {};
        CZOwnership textureOwnership { CZOwnership::Borrow };

        GLuint rbo { 0 };
        CZOwnership rboOwnership { CZOwnership::Borrow };

        GLuint fb { 0 };
        CZOwnership fbOwnership { CZOwnership::Borrow };
        bool hasFb { false };

        EGLImage image { EGL_NO_IMAGE };
        RGLDevice *device { nullptr };
    };

    struct DeviceDataMap : public std::unordered_map<RGLDevice*, DeviceData>
    {
        ~DeviceDataMap() noexcept;
    };

    RGLImage(std::shared_ptr<RCore> core, RGLDevice *device, SkISize size, const RDRMFormat &format) noexcept
        : RImage(core, (RDevice*)device, size, format) {}
    RGLFormat m_glFormat;
    DeviceDataMap m_devicesMap;
};

#endif // RGLIMAGE_H
