#ifndef CZ_RRSCORE_H
#define CZ_RRSCORE_H

#include <CZ/Ream/RCore.h>

struct wl_shm;

class CZ::RRSCore : public RCore
{
public:
    ~RRSCore() noexcept;
    RRSDevice *mainDevice() const noexcept { return (RRSDevice*)m_mainDevice; }
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
