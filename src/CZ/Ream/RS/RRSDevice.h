#ifndef CZ_RRSDEVICE_H
#define CZ_RRSDEVICE_H

#include <CZ/Ream/RDevice.h>

/**
 * @brief Raster (software) implementation of RDevice.
 *
 * RRSDevice is a CPU software-rendering device owned by RRSCore, and can be obtained from an
 * RDevice via asRS(). Depending on the platform it wraps a Wayland connection (no DRM node), a DRM
 * node (opened for dumb-buffer allocation and GBM), or a pure offscreen software device. Its
 * supported texture/render formats are the intersection of the Skia- and DRM-supported formats,
 * always with linear layout.
 */
class CZ::RRSDevice : public RDevice
{
public:
    /**
     * @brief Returns the owning Raster core.
     */
    RRSCore &core() noexcept { return (RRSCore&)m_core; }

    /**
     * @brief No-op for the Raster backend.
     *
     * Software rendering is synchronous, so there is nothing to wait on.
     */
    void wait() noexcept override {};
private:
    friend class RRSCore;
    static RRSDevice *Make(RRSCore &core, int drmFd, void *userData) noexcept;
    RRSDevice(RRSCore &core, int drmFd, void *userData) noexcept;
    ~RRSDevice() noexcept;
    bool init() noexcept;
    bool initWL() noexcept;
    bool initDRM() noexcept;
    bool initOF() noexcept;
    bool initFormats() noexcept;
    std::shared_ptr<RPainter> makePainter(std::shared_ptr<RSurface> surface) noexcept override;
};

#endif // CZ_RRSDEVICE_H
