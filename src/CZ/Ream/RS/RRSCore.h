#ifndef CZ_RRSCORE_H
#define CZ_RRSCORE_H

#include <CZ/Ream/RCore.h>

struct wl_shm;

/**
 * @brief Raster (software) implementation of RCore.
 *
 * RRSCore is the CPU software-rendering backend of Ream's global context. It is selected when the
 * graphics API resolves to RGraphicsAPI::RS, and can be obtained from an RCore via asRS().
 *
 * On the Wayland platform it binds the compositor's @c wl_shm global (used to allocate shared-memory
 * swapchain buffers), then enumerates its rendering devices (RRSDevice). On DRM and Offscreen
 * platforms only the devices are set up.
 */
class CZ::RRSCore : public RCore
{
public:
    /**
     * @brief Destroys the core, releasing its devices and the bound @c wl_shm global (if any).
     */
    ~RRSCore() noexcept;

    /**
     * @brief Returns the main Raster device.
     *
     * Same as RCore::mainDevice() but downcast to the Raster backend type.
     */
    RRSDevice *mainDevice() const noexcept { return (RRSDevice*)m_mainDevice; }

    /**
     * @brief No-op for the Raster backend.
     *
     * The software backend keeps no deferred-destruction garbage, so this override does nothing.
     */
    void clearGarbage() noexcept override {};
private:
    friend class RCore;
    friend class RRSSwapchainWL;
    bool init() noexcept override;
    bool initWL() noexcept;
    bool initDevices() noexcept;
    void unitDevices() noexcept;
    RRSCore(const Options &options) noexcept;
    wl_shm *m_shm { nullptr };
};

#endif // CZ_RRSCORE_H
