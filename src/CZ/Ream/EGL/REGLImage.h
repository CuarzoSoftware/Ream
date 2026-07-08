#ifndef REGLIMAGE_H
#define REGLIMAGE_H

#include <CZ/Ream/GL/RGLTexture.h>
#include <CZ/Ream/GL/RGLContext.h>
#include <CZ/Ream/RObject.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <memory>

/**
 * @brief Wraps an EGLImage imported from a dma-buf, exposing it to the OpenGL backend.
 *
 * Created from an RDMABufferInfo via eglCreateImageKHR (EGL_LINUX_DMA_BUF_EXT), it can be bound as
 * a GL texture (GL_TEXTURE_2D or GL_TEXTURE_EXTERNAL_OES depending on the format's render support)
 * or as a renderbuffer-backed framebuffer. The GL texture/framebuffer objects are created lazily
 * and per-device (via the context-data manager).
 */
class CZ::REGLImage final : public RObject
{
public:
    /**
     * @brief Imports @p info as an EGLImage on @p device (or the main device if null).
     *
     * The plane fds are dup'd internally, so ownership is not taken. Returns nullptr if the
     * format/modifier is unsupported or eglCreateImageKHR fails.
     */
    static std::shared_ptr<REGLImage> MakeFromDMA(const RDMABufferInfo &info, RGLDevice *device = nullptr) noexcept;

    /**
     * @brief Returns a GL texture bound to this EGLImage (created lazily).
     * @return The texture, with id == 0 on failure.
     */
    RGLTexture texture() const noexcept;

    /**
     * @brief Returns a GL framebuffer (renderbuffer-backed by this EGLImage), created lazily per device.
     * @return The framebuffer id, or 0 on failure.
     */
    GLuint fb() const noexcept;

    /** @brief The underlying EGLImage handle. */
    EGLImage eglImage() const noexcept { return m_eglImage; }

    /** @brief The device this EGLImage was created on. */
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
