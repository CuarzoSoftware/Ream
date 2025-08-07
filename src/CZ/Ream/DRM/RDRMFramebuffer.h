#ifndef RDRMFRAMEBUFFER_H
#define RDRMFRAMEBUFFER_H

#include <memory>
#include <CZ/Ream/RObject.h>
#include <CZ/skia/core/SkSize.h>
#include <CZ/CZOwn.h>

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

    static std::shared_ptr<RDRMFramebuffer> WrapHandle(SkISize size, UInt32 stride, RFormat format, RModifier modifier, UInt32 handle,
                                                       CZOwn ownership, RDevice *device) noexcept;

    bool hasModifier() const noexcept { return m_hasModifier; };
    RModifier modifier() const noexcept { return m_modifier; }
    RFormat format() const noexcept { return m_format; }
    UInt32 id() const noexcept { return m_id; }
    RDevice *device() const noexcept { return m_device; }
    std::shared_ptr<RCore> core() const noexcept { return m_core; }
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
