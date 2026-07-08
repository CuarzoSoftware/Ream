#ifndef RDUMBBUFFER_H
#define RDUMBBUFFER_H

#include <CZ/Ream/RObject.h>
#include <CZ/Ream/DRM/RDRMFormat.h>
#include <CZ/skia/core/SkSize.h>
#include <memory>

/**
 * @brief A DRM dumb buffer for software (CPU) rendering.
 *
 * Wraps a DRM dumb buffer (`drmModeCreateDumbBuffer`), a simple CPU-accessible buffer suitable
 * for software rendering and scanout on devices without GPU acceleration. The buffer is mapped
 * into the process address space (via `mmap`) so its pixels can be read and written directly,
 * and it is also wrapped in an RDRMFramebuffer so it can be presented via KMS.
 *
 * Dumb buffers only support the DRM_FORMAT_MOD_INVALID or DRM_FORMAT_MOD_LINEAR modifier, and
 * only single-block (1x1 pixel-per-block) formats. The allocating device must advertise the
 * DumbBuffer capability.
 *
 * Use Make() to create an instance.
 */
class CZ::RDumbBuffer final : public RObject
{
public:
    /**
     * @brief Creates and maps a DRM dumb buffer.
     *
     * @param size      The width and height of the buffer in pixels; must be non-empty.
     * @param format    The DRM format and modifiers. The modifier set must contain
     *                  DRM_FORMAT_MOD_INVALID or DRM_FORMAT_MOD_LINEAR, and the format must be a
     *                  supported single-block (1x1) format.
     * @param allocator The DRM device to allocate on. If nullptr, RCore::mainDevice() is used.
     *                  Must support the DumbBuffer capability.
     *
     * @return A shared pointer to a valid RDumbBuffer on success, nullptr on failure.
     */
    static std::shared_ptr<RDumbBuffer> Make(SkISize size, const RDRMFormat &format, RDevice *allocator = nullptr) noexcept;

    /**
     * @brief Unmaps and destroys the underlying dumb buffer.
     */
    ~RDumbBuffer() noexcept;

    /**
     * @brief Returns the internal shared reference to the core instance.
     *
     * This ensures the current RCore instance stays alive while the buffer exists.
     */
    std::shared_ptr<RCore> core() const noexcept { return m_core; }

    /**
     * @brief Returns the DRM framebuffer wrapping this dumb buffer, for KMS scanout.
     */
    std::shared_ptr<RDRMFramebuffer> fb() const noexcept;

    /**
     * @brief Returns the width and height of the buffer in pixels.
     */
    SkISize size() const noexcept { return m_size; }

    /**
     * @brief Returns detailed information about the buffer's DRM format.
     */
    const RFormatInfo &formatInfo() const noexcept { return *m_formatInfo; }

    /**
     * @brief Returns the DRM device that allocated this buffer.
     */
    RDevice *allocator() const noexcept { return m_allocator; }

    /**
     * @brief Returns the GEM handle of the dumb buffer, valid on the allocator device.
     */
    UInt32 handle() const noexcept { return m_handle; }

    /**
     * @brief Returns the buffer stride (bytes per row).
     */
    UInt32 stride() const noexcept { return m_stride; }

    /**
     * @brief Returns a pointer to the mapped, CPU-accessible pixel data.
     *
     * The mapping is read/write and remains valid for the lifetime of this object.
     */
    UInt8 *pixels() const noexcept { return m_data; }
private:
    RDumbBuffer(std::shared_ptr<RCore> core, std::shared_ptr<RDRMFramebuffer> fb, SkISize size, const RFormatInfo *formatInfo, RDevice *allocator, UInt32 handle,
                UInt32 stride, UInt64 mapSize, UInt8 *data) noexcept;
    std::shared_ptr<RCore> m_core;
    std::shared_ptr<RDRMFramebuffer> m_fb;
    SkISize m_size;
    const RFormatInfo *m_formatInfo;
    RDevice *m_allocator;
    UInt32 m_handle;
    UInt32 m_stride;
    UInt64 m_mapSize;
    UInt8 *m_data;
};

#endif // RDUMBBUFFER_H
