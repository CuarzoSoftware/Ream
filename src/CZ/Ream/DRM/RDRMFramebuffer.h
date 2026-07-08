#ifndef RDRMFRAMEBUFFER_H
#define RDRMFRAMEBUFFER_H

#include <memory>
#include <CZ/Ream/RObject.h>
#include <CZ/skia/core/SkSize.h>
#include <CZ/Core/CZOwn.h>

/**
 * @brief A DRM framebuffer usable for KMS scanout.
 *
 * Wraps a DRM framebuffer object created via `drmModeAddFB2` (with a fallback to
 * `drmModeAddFB2WithModifiers` or the legacy `drmModeAddFB` when necessary). A framebuffer
 * binds one or more GEM buffer handles, together with their strides, offsets, format and
 * modifier, into a single object that can be presented on a CRTC/plane.
 *
 * Instances are created through the static factory methods from an existing GBM buffer object
 * (MakeFromGBMBo()), imported DMA-BUF file descriptors (MakeFromDMA()), or a raw GEM handle
 * (WrapHandle()). The framebuffer is always tied to the DRM device that created it.
 *
 * On destruction the underlying framebuffer is removed (`drmModeCloseFB`, falling back to
 * `drmModeRmFB`), and any GEM handles owned by this object (CZOwn::Own) are closed.
 */
class CZ::RDRMFramebuffer final : public RObject
{
public:
    /**
     * @brief Creates a framebuffer from a GBM buffer object (bo).
     *
     * This creates a framebuffer that can be used by the DRM device which allocated the provided GBM buffer object.
     *
     * @param bo A shared pointer to the GBM buffer object.
     *
     * @return A shared pointer to the created RDRMFramebuffer instance, or nullptr on failure.
     *
     * @note The buffer object is retained internally and will remain alive until the framebuffer is destroyed.
     */
    static std::shared_ptr<RDRMFramebuffer> MakeFromGBMBo(std::shared_ptr<RGBMBo> bo) noexcept;

    /**
     * @brief Creates a framebuffer from DMA-BUF file descriptors.
     *
     * Does not take ownership of the provided file descriptors.
     *
     * @note The DMA-BUF file descriptors can be safely closed after this call,
     *       as this object retains a reference to the buffers via GEM handles.
     *
     * @param dmaInfo Information describing the DMA-BUF to import.
     * @param importer The DRM device that will import the DMA-BUF fds and use the framebuffer.
     *                 If nullptr, RCore::mainDevice() is used instead.
     *
     * @return A shared pointer to the created RDRMFramebuffer instance, or nullptr on failure.
     */
    static std::shared_ptr<RDRMFramebuffer> MakeFromDMA(const RDMABufferInfo &dmaInfo, RDevice *importer = nullptr) noexcept;

    /**
     * @brief Creates a single-plane framebuffer from an existing GEM buffer handle.
     *
     * @param size      The width and height of the framebuffer in pixels.
     * @param stride    The stride (bytes per row) of the buffer.
     * @param format    The DRM pixel format.
     * @param modifier  The DRM format modifier of the buffer.
     * @param handle    The GEM handle of the buffer, valid on @p device.
     * @param ownership CZOwn::Own to take ownership of the handle (closed when this object is
     *                  destroyed, including on failure), or CZOwn::Borrow to leave it untouched.
     * @param device    The DRM device that owns the handle and will use the framebuffer.
     *                  If nullptr, RCore::mainDevice() is used instead.
     *
     * @return A shared pointer to the created RDRMFramebuffer instance, or nullptr on failure.
     */
    static std::shared_ptr<RDRMFramebuffer> WrapHandle(SkISize size, UInt32 stride, RFormat format, RModifier modifier, UInt32 handle,
                                                       CZOwn ownership, RDevice *device) noexcept;

    /**
     * @brief Whether the framebuffer was created with an explicit format modifier.
     *
     * @return true if an explicit modifier was used, false otherwise (in which case modifier()
     *         may not describe the actual buffer layout).
     */
    bool hasModifier() const noexcept { return m_hasModifier; };

    /**
     * @brief Returns the DRM format modifier of the framebuffer.
     *
     * @see hasModifier()
     */
    RModifier modifier() const noexcept { return m_modifier; }

    /**
     * @brief Returns the DRM pixel format of the framebuffer.
     */
    RFormat format() const noexcept { return m_format; }

    /**
     * @brief Returns the DRM framebuffer id, as used by KMS APIs.
     */
    UInt32 id() const noexcept { return m_id; }

    /**
     * @brief Returns the DRM device that owns this framebuffer.
     */
    RDevice *device() const noexcept { return m_device; }

    /**
     * @brief Returns the internal shared reference to the core instance.
     *
     * This ensures the current RCore instance stays alive while the framebuffer exists.
     */
    std::shared_ptr<RCore> core() const noexcept { return m_core; }

    /**
     * @brief Destroys the framebuffer and closes any owned GEM handles.
     */
    ~RDRMFramebuffer() noexcept;
private:
    RDRMFramebuffer(std::shared_ptr<RCore> core,
                    RDevice *device, UInt32 id,
                    SkISize size, RFormat format,
                    RModifier modifier, bool hasModifier,
                    const std::vector<UInt32> &handles,
                    CZOwn handlesOwnership) noexcept;
    UInt32 m_id;
    SkISize m_size {};
    RDevice *m_device;
    RFormat m_format;
    RModifier m_modifier;
    std::shared_ptr<RGBMBo> m_gbmBo;
    std::shared_ptr<RCore> m_core;
    std::vector<UInt32> m_handles;
    CZOwn m_handlesOwn;
    bool m_hasModifier;
};

#endif // RDRMFRAMEBUFFER_H
