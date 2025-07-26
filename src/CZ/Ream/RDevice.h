#ifndef RDEVICE_H
#define RDEVICE_H

#include <CZ/Ream/DRM/RDRMFormat.h>
#include <CZ/Ream/RLog.h>
#include <CZ/Ream/RObject.h>
#include <CZ/CZWeak.h>
#include <string>

namespace CZ
{
    class SRMDevice;

    enum class RDriver
    {
        unknown,
        amdgpu,
        i915,
        nouveau,
        nvidia,
        lima,
        radeon
    };
}

struct gbm_device;

class CZ::RDevice : public RObject
{
public:

    /**
     * @brief Device capabilities.
     */
    struct Caps
    {
        /// Supports creating DRM framebuffers with explicit format modifiers.
        bool AddFb2Modifiers;

        /// Supports RSync-based client-side synchronization.
        bool SyncCPU;

        /// Supports RSync-based host-side synchronization.
        bool SyncGPU;

        /// Supports RSync synchronization with external devices.
        bool SyncImport;
        bool SyncExport;
    };

    ~RDevice() noexcept;

    // Could be -1
    int drmFd() const noexcept { return m_drmFd; }

    void *drmUserData() const noexcept { return m_drmUserData; }

    // e.g. /dev/dri/card0 or empty
    const std::string &drmNode() const noexcept { return m_drmNode; }
    const std::string &drmDriverName() const noexcept { return m_drmDriverName; }
    RDriver drmDriver() const noexcept { return m_driver; }

    // Could be nullptr
    gbm_device *gbmDevice() const noexcept { return m_gbmDevice; }

    // Could be nullptr
    SRMDevice *srmDevice() const noexcept { return m_srmDevice; }

    RCore &core() const noexcept { return m_core; }
    RGLDevice *asGL() noexcept;

    const Caps &caps() const noexcept { return m_caps; }

    // Formats than can be used as a source
    const RDRMFormatSet &dmaTextureFormats() const noexcept { return m_dmaTextureFormats; }

    // Formats that can be used as a render destination
    // If a format/modifier pair exists in textures but not here, then its external only
    const RDRMFormatSet &dmaRenderFormats() const noexcept { return m_dmaRenderFormats; }

    CZLogger log { RLog };
protected:
    friend class SRMCore;
    friend class RSurface;
    virtual RPainter *painter() const noexcept = 0;
    RDevice(RCore &core) noexcept;;
    RCore &m_core;
    int m_drmFd { -1 };
    void *m_drmUserData {};
    Caps m_caps {};
    std::string m_drmNode;
    std::string m_drmDriverName { "Unknown" };
    RDriver m_driver { RDriver::unknown };
    gbm_device *m_gbmDevice { nullptr };
    RDRMFormatSet m_dmaTextureFormats;
    RDRMFormatSet m_dmaRenderFormats;
    CZWeak<SRMDevice> m_srmDevice;
};

#endif // RDEVICE_H
