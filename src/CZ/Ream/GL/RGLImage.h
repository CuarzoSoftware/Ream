#ifndef RGLIMAGE_H
#define RGLIMAGE_H

#include <CZ/Core/CZBitset.h>
#include <CZ/Ream/GL/RGLTexture.h>
#include <CZ/Ream/RImage.h>
#include <CZ/Ream/GL/RGLFormat.h>
#include <CZ/Ream/GL/RGLContext.h>
#include <EGL/egl.h>
#include <unordered_map>

namespace CZ
{
    /**
     * @brief Describes an existing OpenGL framebuffer object to wrap as an RGLImage.
     *
     * Used by RGLImage::BorrowFramebuffer() to build an image backed by a framebuffer that
     * Ream does not own.
     */
    struct RGLFramebufferInfo
    {
        GLuint id;                                      ///< The GL framebuffer object name (0 refers to the default framebuffer).
        SkISize size {};                                ///< Framebuffer size in pixels.
        RFormat format;                                 ///< DRM fourcc format of the framebuffer's color attachment.
        SkAlphaType alphaType { kUnknown_SkAlphaType }; ///< Alpha interpretation of the pixels.
    };

    /**
     * @brief Describes an existing EGLSurface to wrap as an RGLImage.
     *
     * Used by RGLImage::FromEGLSurface() to build an image that renders into a window/pbuffer
     * EGLSurface (e.g. a Wayland swapchain surface).
     */
    struct REGLSurfaceInfo
    {
        EGLSurface surface { EGL_NO_SURFACE };          ///< The EGL surface to render into.
        SkISize size {};                                ///< Surface size in pixels.
        RFormat format;                                 ///< DRM fourcc format of the surface.
        SkAlphaType alphaType { kUnknown_SkAlphaType }; ///< Alpha interpretation of the pixels.
    };
}

/**
 * @brief OpenGL backend implementation of RImage.
 *
 * Backs a Ream image with OpenGL ES/EGL resources. Depending on how it was created, storage is
 * either GBM/DMA-buf (importable across devices as an EGLImage/texture) or native GL. GL objects
 * (textures, framebuffers, EGLImages, Skia images/surfaces) are created lazily per device and, for
 * context-specific objects, per GL context, and cached.
 *
 * Obtain it from an RImage via RImage::asGL().
 */
class CZ::RGLImage : public RImage
{
public:
    /**
     * @brief Returns the OpenGL texture id and target for the given device.
     *
     * The texture is created lazily by importing the image's EGLImage on first use and cached
     * per device.
     *
     * @param device Target device, or `nullptr` for the main device.
     * @returns {0, 0} if no device is bound or the image couldn't be imported.
     */
    RGLTexture texture(RGLDevice *device = nullptr) const noexcept;

    /**
     * @brief Returns a GL framebuffer object that renders into this image, for the given device.
     *
     * Created lazily and cached per GL context: for EGLSurface-backed images this is the default
     * framebuffer (0); for native storage a new FBO is generated and the texture attached; otherwise
     * the framebuffer is obtained from the backing EGLImage.
     *
     * @param device Target device, or `nullptr` for the main device.
     * @return The framebuffer name, or `std::nullopt` if the image cannot be used as a render target.
     */
    std::optional<GLuint> glFb(RGLDevice *device = nullptr) const noexcept;

    /**
     * @brief Returns the EGLImage backing this image on the given device.
     *
     * Created lazily by exporting the backing buffer as a DMA-buf and importing it as an EGLImage;
     * cached per device.
     *
     * @param device Target device, or `nullptr` for the main device.
     * @return The EGLImage, or `nullptr` if one could not be created.
     */
    std::shared_ptr<REGLImage> eglImage(RGLDevice *device = nullptr) const noexcept;

    /**
     * @brief Returns the EGLSurface this image renders into, if any.
     *
     * Only images created via FromEGLSurface() are backed by a surface.
     *
     * @param device Target device, or `nullptr` for the main device.
     * @return The EGL surface, or EGL_NO_SURFACE if the image is not surface-backed.
     */
    EGLSurface eglSurface(RGLDevice *device = nullptr) const noexcept;

    /**
     * @brief Creates an OpenGL-backed image.
     *
     * Attempts native GL storage first, falling back to GBM/DMA-buf storage.
     *
     * @param size        Image size in pixels.
     * @param format      Requested DRM format(s) and modifiers.
     * @param constraints Optional capability/format constraints.
     * @return The new image, or `nullptr` on failure.
     */
    [[nodiscard]] static std::shared_ptr<RGLImage> Make(SkISize size, const RDRMFormat &format, const RImageConstraints *constraints = nullptr) noexcept;

    /**
     * @brief Creates an image by importing an existing DMA-buffer.
     *
     * @param info        Description of the DMA-buffer to import.
     * @param ownership   Whether Ream takes ownership of the buffer's file descriptors.
     * @param constraints Optional capability/format constraints.
     * @return The new image, or `nullptr` on failure.
     */
    [[nodiscard]] static std::shared_ptr<RGLImage> FromDMA(const RDMABufferInfo &info, CZOwn ownership, const RImageConstraints *constraints = nullptr) noexcept;

    /**
     * @brief Wraps an existing OpenGL framebuffer as an image.
     *
     * @param info      Description of the framebuffer to wrap.
     * @param allocator Owning device, or `nullptr` for the main device.
     * @return The new image, or `nullptr` on failure.
     */
    [[nodiscard]] static std::shared_ptr<RGLImage> BorrowFramebuffer(const RGLFramebufferInfo &info, RGLDevice *allocator = nullptr) noexcept;

    /**
     * @brief Wraps an existing EGLSurface as an image.
     *
     * @param info      Description of the EGL surface to wrap.
     * @param ownership Whether Ream takes ownership of the EGL surface.
     * @param allocator Owning device, or `nullptr` for the main device.
     * @return The new image, or `nullptr` on failure.
     */
    [[nodiscard]] static std::shared_ptr<RGLImage> FromEGLSurface(const REGLSurfaceInfo &info, CZOwn ownership, RGLDevice *allocator = nullptr) noexcept;

    std::shared_ptr<RGBMBo> gbmBo(RDevice *device = nullptr) const noexcept override;
    std::shared_ptr<RDRMFramebuffer> drmFb(RDevice *device = nullptr) const noexcept override;
    sk_sp<SkImage> skImage(RDevice *device = nullptr) const noexcept override;
    sk_sp<SkSurface> skSurface(RDevice *device = nullptr) const noexcept override;
    CZBitset<RImageCap> checkDeviceCaps(CZBitset<RImageCap> caps, RDevice *device = nullptr) const noexcept override;

    bool writePixels(const RPixelBufferRegion &region) noexcept override;
    bool readPixels(const RPixelBufferRegion &region) noexcept override;

    /**
     * @brief Returns the device that allocated this image as an RGLDevice.
     */
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

        EGLSurface eglSurface { EGL_NO_SURFACE };
        CZOwn eglSurfaceOwn { CZOwn::Borrow };

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
