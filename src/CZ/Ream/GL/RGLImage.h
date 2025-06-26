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
}

class CZ::RGLImage : public RImage
{
public:
    static std::shared_ptr<RGLImage> MakeFromPixels(const MakeFromPixelsParams &params, RGLDevice *allocator = nullptr) noexcept;

private:
    struct DeviceData
    {
        RGLTexture texture {};
        GLuint rbo;
        GLuint fb;
        EGLImage image { EGL_NO_IMAGE };
        RGLDevice *device;
    };

    RGLImage();
    RGLFormat m_glFormat;
    std::unordered_map<RGLDevice*, DeviceData> m_devicesData;
};

#endif // RGLIMAGE_H
