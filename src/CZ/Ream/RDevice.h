#ifndef RDEVICE_H
#define RDEVICE_H

#include <CZ/Ream/DRM/RDRMFormat.h>
#include <CZ/Ream/RLog.h>
#include <CZ/Ream/RObject.h>
#include <CZ/Core/CZWeak.h>
#include <string>

namespace CZ
{
    class SRMDevice;

    /**
     * @brief Known DRM kernel drivers.
     *
     * Identifies the driver backing an RDevice, detected from the DRM version name.
     */
    enum class RDriver
    {
        unknown, ///< Driver could not be identified.
        amdgpu,  ///< AMD GPUs (amdgpu).
        i915,    ///< Intel GPUs (i915).
        nouveau, ///< NVIDIA GPUs, open-source driver (nouveau).
        nvidia,  ///< NVIDIA GPUs, proprietary driver (nvidia / nvidia-drm).
        lima,    ///< ARM Mali Utgard GPUs (lima).
        radeon   ///< Older AMD/ATI GPUs (radeon).
    };
}

struct gbm_device;

/**
 * @brief A rendering device.
 *
 * An RDevice represents a single GPU (or software renderer) usable for allocation and rendering.
 * There is typically one device per DRM node; systems with multiple GPUs expose several, enabling
 * hybrid-GPU (Prime) setups where buffers are allocated on one device and sampled on another via
 * DMA-buf sharing.
 *
 * Devices are enumerated and owned by RCore. Each exposes its capabilities (RDevice::Caps) and the
 * texture/render/DMA formats it supports. Use RCore::mainDevice() for the primary device.
 */
class CZ::RDevice : public RObject
{
public:

    /**
     * @brief Device capabilities.
     */
    struct Caps
    {
        /// Supports rendering (as opposed to allocation/scanout only).
        bool Rendering;

        /// Supports creating DRM framebuffers with explicit format modifiers.
        bool AddFb2Modifiers;

        /// Supports allocating dumb buffers.
        bool DumbBuffer;

        /// Supports RSync-based client-side synchronization.
        bool SyncCPU;

        /// Supports RSync-based host-side synchronization.
        bool SyncGPU;

        /// Supports RSync synchronization with external devices.
        bool SyncImport;

        /// Supports exporting RSync fences to external devices.
        bool SyncExport;

        /// Supports timeline (as opposed to binary) synchronization.
        bool Timeline;
    };

    /**
     * @brief Destructor.
     *
     * Destroys the GBM device and closes the DRM file descriptors it owns.
     */
    ~RDevice() noexcept;

    /**
     * @brief Blocks until the device has finished all pending work.
     */
    virtual void wait() noexcept = 0;

    // Can be -1
    /**
     * @brief Returns the primary DRM file descriptor.
     *
     * @return The DRM fd, or -1 if none is available.
     */
    int drmFd() const noexcept { return m_drmFd; }

    /**
     * @brief Returns the read-only DRM file descriptor.
     *
     * @return The read-only DRM fd, or -1 if none is available.
     */
    int drmFdReadOnly() const noexcept { return m_drmFdReadOnly; }

    /**
     * @brief Returns the user data associated with the DRM device.
     */
    void *drmUserData() const noexcept { return m_drmUserData; }

    /**
     * @brief Returns the device id (dev_t) of the DRM node.
     */
    dev_t id() const noexcept { return m_id; }

    // e.g. /dev/dri/card0 or empty
    /**
     * @brief Returns the path of the DRM node.
     *
     * @return The node path (e.g. @c /dev/dri/card0), or an empty string if none.
     */
    const std::string &drmNode() const noexcept { return m_drmNode; }

    /**
     * @brief Returns the DRM driver name.
     *
     * @return The driver name (e.g. @c "amdgpu"), or @c "Unknown" if not detected.
     */
    const std::string &drmDriverName() const noexcept { return m_drmDriverName; }

    /**
     * @brief Returns the detected DRM driver.
     */
    RDriver drmDriver() const noexcept { return m_driver; }

    // Could be nullptr
    gbm_device *gbmDevice() const noexcept { return m_gbmDevice; }

    // Could be nullptr
    SRMDevice *srmDevice() const noexcept { return m_srmDevice; }

    /**
     * @brief Returns the RCore instance that owns this device.
     */
    RCore &core() const noexcept { return m_core; }

    /**
     * @brief Attempts to cast this device to an RGLDevice.
     *
     * @return A pointer to RGLDevice if the active API is OpenGL, or nullptr otherwise.
     */
    RGLDevice *asGL() noexcept;

    /**
     * @brief Attempts to cast this device to an RRSDevice.
     *
     * @return A pointer to RRSDevice if the active API is Raster, or nullptr otherwise.
     */
    RRSDevice *asRS() noexcept;

    /**
     * @brief Attempts to cast this device to an RVKDevice.
     *
     * @return A pointer to RVKDevice if the active API is Vulkan, or nullptr otherwise.
     */
    RVKDevice *asVK() noexcept;

    /**
     * @brief Returns the capabilities of this device.
     */
    const Caps &caps() const noexcept { return m_caps; }

    // DMA formats than can be used as a source
    const RDRMFormatSet &dmaTextureFormats() const noexcept { return m_dmaTextureFormats; }

    // DMA formats than can be used as a destination
    const RDRMFormatSet &dmaRenderFormats() const noexcept { return m_dmaRenderFormats; }

    // DMA + Native ones
    const RDRMFormatSet &textureFormats() const noexcept { return m_textureFormats; }
    const RDRMFormatSet &renderFormats() const noexcept { return m_renderFormats; }

    CZWeak<CZObject> drmLeaseGlobal; // Reserved for Louvre's DRM lease globals

    /// Logger for messages related to this device.
    CZLogger log { RLog };
protected:
    friend class SRMCore;
    friend class RSurface;
    friend class RPass;
    virtual std::shared_ptr<RPainter> makePainter(std::shared_ptr<RSurface> surface) noexcept = 0;
    RDevice(RCore &core) noexcept;
    void setDRMDriverName(int fd) noexcept;
    RCore &m_core;
    int m_drmFd { -1 };
    int m_drmFdReadOnly { -1 };
    void *m_drmUserData {};
    dev_t m_id {};
    Caps m_caps {};
    std::string m_drmNode;
    std::string m_drmDriverName { "Unknown" };
    RDriver m_driver { RDriver::unknown };
    gbm_device *m_gbmDevice { nullptr };
    RDRMFormatSet m_dmaTextureFormats;
    RDRMFormatSet m_dmaRenderFormats;
    RDRMFormatSet m_textureFormats;
    RDRMFormatSet m_renderFormats;
    CZWeak<SRMDevice> m_srmDevice;
};

#endif // RDEVICE_H
