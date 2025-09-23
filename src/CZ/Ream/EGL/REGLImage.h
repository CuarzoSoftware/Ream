#ifndef REGLIMAGE_H
#define REGLIMAGE_H

#include <CZ/Ream/GL/RGLTexture.h>
#include <CZ/Ream/GL/RGLContext.h>
#include <CZ/Ream/RObject.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <memory>

class CZ::REGLImage final : public RObject
{
public:
    // dups the fds internally (does not take ownership)
    static std::shared_ptr<REGLImage> MakeFromDMA(const RDMABufferInfo &info, RGLDevice *device = nullptr) noexcept;

    // id = 0 on failure
    RGLTexture texture() const noexcept;

    // 0 on failure
    GLuint fb() const noexcept;

    EGLImage eglImage() const noexcept { return m_eglImage; }
    RGLDevice *device() const noexcept { return m_device; }
    ~REGLImage() noexcept;
private:
    REGLImage(std::shared_ptr<RGLCore> core, RGLDevice *device, EGLImage image, GLenum target) noexcept;

    class CtxData : public RGLContextData
    {
    public:
        CtxData(RGLDevice *device) noexcept : device(device) {};
        ~CtxData() noexcept;
        GLuint rbo { 0 };
        GLuint fb { 0 };
        RGLDevice *device { nullptr };
    };

    std::shared_ptr<RGLCore> m_core;
    EGLImage m_eglImage;
    mutable RGLTexture m_tex {};
    RGLDevice *m_device;
    std::shared_ptr<RGLContextDataManager> m_ctxDataManager;
};

#endif // REGLIMAGE_H
