#ifndef CZ_RRSDEVICE_H
#define CZ_RRSDEVICE_H

#include <CZ/Ream/RDevice.h>
#include <thread>

class CZ::RRSDevice : public RDevice
{
public:
    RRSCore &core() noexcept { return (RRSCore&)m_core; }
    void wait() noexcept override {};
private:
    friend class RRSCore;
    static RRSDevice *Make(RRSCore &core, int drmFd, void *userData) noexcept;
    RRSDevice(RRSCore &core, int drmFd, void *userData) noexcept;
    ~RRSDevice() noexcept;
    bool init() noexcept;
    bool initWL() noexcept;
    bool initDRM() noexcept;
    bool initFormats() noexcept;
    RPainter *painter() const noexcept override;
    mutable std::unordered_map<std::thread::id, std::shared_ptr<RRSPainter>> m_painters;
};

#endif // CZ_RRSDEVICE_H
