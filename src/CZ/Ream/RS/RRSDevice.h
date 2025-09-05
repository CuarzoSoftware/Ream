#ifndef CZ_RRSDEVICE_H
#define CZ_RRSDEVICE_H

#include <CZ/Ream/RDevice.h>

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
    bool initOF() noexcept;
    bool initFormats() noexcept;
    std::shared_ptr<RPainter> makePainter(std::shared_ptr<RSurface> surface) noexcept override;
};

#endif // CZ_RRSDEVICE_H
