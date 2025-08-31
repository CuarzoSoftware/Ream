#ifndef RGBMBO_H
#define RGBMBO_H

#include <CZ/Core/Cuarzo.h>
#include <CZ/Core/CZOwn.h>
#include <CZ/Ream/RObject.h>
#include <CZ/Ream/RDMABufferInfo.h>
#include <memory>
#include <gbm.h>

/**
 * @brief Wrapper around a native GBM buffer object (gbm_bo).
 *
 * This class encapsulates a GBM buffer object, providing safe access to its
 * properties and DMA-BUF export information. It manages the lifetime of the
 * gbm_bo handle and related resources.
 *
 * Use Make() to create an instance. The buffer object is created either with
 * or without modifiers, depending on platform support and requested format.
 */
class CZ::RGBMBo : public RObject
{
public:

    ~RGBMBo() noexcept;

    /**
     * @brief Creates a GBM buffer object.
     *
     * Allocates a new `gbm_bo` with the specified size and format.
     *
     * If no allocator is provided, the default device from `RCore::mainDevice()` is used.
     *
     * @param size      The width and height of the buffer.
     * @param format    The DRM pixel format and modifiers.
     * @param allocator The device to use for allocation (optional).
     *
     * @return A shared pointer to a valid RGBMBo on success, nullptr on failure.
     */
    static std::shared_ptr<RGBMBo> Make(SkISize size, const RDRMFormat &format, RDevice *allocator = nullptr) noexcept;

    // For hw cursors, gbm_bo_write is garanteed to work
    static std::shared_ptr<RGBMBo> MakeCursor(SkISize size, RFormat format, RDevice *allocator = nullptr) noexcept;

    // Does not take ownership of the fds (dups them internally)
    static std::shared_ptr<RGBMBo> MakeFromDMA(const RDMABufferInfo &dmaInfo, RDevice *importer = nullptr) noexcept;

    /**
     * @brief Returns DMA-BUF export information.
     *
     * @note File descriptors in the returned structure are owned by this object and must not be closed.
     *       If another entity takes ownership of the fds you must duplicate them first.
     *
     * @return A reference to the DMA buffer info. The fds are always valid except for bos created with MakeCursor.
     */
    const RDMABufferInfo &dmaInfo() const noexcept { return m_dmaInfo; }

    /**
     * @brief Returns the allocator device used to create this buffer.
     */
    RDevice *allocator() const noexcept { return m_allocator; }

    /**
     * @brief Checks whether the buffer was created with an explicit modifier.
     *
     * If true, the buffer was created using `gbm_bo_create_with_modifiers` and has a valid modifier.
     * If false, it was created using `gbm_bo_create`, and the modifier may be
     * `DRM_FORMAT_MOD_INVALID` or `DRM_FORMAT_MOD_LINEAR`.
     *
     * @return True if the buffer has an explicit modifier, false otherwise.
     */
    bool hasModifier() const noexcept { return m_hasModifier; }

    /**
     * @brief Returns the DRM format modifier of the buffer.
     *
     * @see hasModifier()
     */
    RModifier modifier() const noexcept { return m_dmaInfo.modifier; }

    /**
     * @brief Returns the number of planes in the buffer.
     *
     * @return Number of planes (usually 1 for RGB, may be more for YUV).
     */
    int planeCount() const noexcept { return m_dmaInfo.planeCount; }

    /**
     * @brief Returns the GEM flink handle for a specific plane.
     *
     * @note Handles must not be manually closed or used by other device than the allocator.
     *       For cross-device sharing, use dmaInfo().
     *
     * @param planeIndex Index of the plane (0-based).
     * @return The GEM flink handle (DRM buffer object handle) for the specified plane.
     */
    union gbm_bo_handle planeHandle(int planeIndex) const noexcept;

    /**
     * @brief Checks whether the buffer supports memory mapping for read access.
     *
     * @note The mapping present a linear view of the buffer, independently of the modifier.
     *
     * @return True if `gbm_bo_map` can be used for reading, false otherwise.
     */
    bool supportsMapRead() const noexcept;

    /**
     * @brief Checks whether the buffer supports memory mapping for write access.
     *
     * @note The mapping present a linear view of the buffer, independently of the modifier.
     *
     * @return True if `gbm_bo_map` can be used for writing, false otherwise.
     */
    bool supportsMapWrite() const noexcept;

    /**
     * @brief Returns the raw `gbm_bo` handle.
     *
     * @note This handle is owned by the object. Do not manually destroy it.
     *
     * @return Pointer to the underlying gbm_bo.
     */
    gbm_bo *bo() const noexcept { return m_bo; }

    /**
     * @brief Returns the internal shared reference to the core instance.
     *
     * This ensures the current RCore instance stays alive while the buffer exists.
     */
    std::shared_ptr<RCore> core() const noexcept { return m_core; }
private:
    RGBMBo(std::shared_ptr<RCore> core, RDevice *allocator, gbm_bo *bo, CZOwn ownership, bool hasModifier, const RDMABufferInfo &dmaInfo) noexcept;
    gbm_bo *m_bo;
    RDMABufferInfo m_dmaInfo;
    CZOwn m_ownership;
    RDevice *m_allocator;
    std::shared_ptr<RCore> m_core;
    // 2: unchecked, 1: supported, 0: unsupported
    mutable UInt8 m_supportsMapRead { 2 };
    mutable UInt8 m_supportsMapWrite { 2 };
    bool m_hasModifier;
};

#endif // RGBMBO_H
