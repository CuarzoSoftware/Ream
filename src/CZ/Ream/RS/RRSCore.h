#ifndef CZ_RRSCORE_H
#define CZ_RRSCORE_H

#include <CZ/Ream/RCore.h>

class CZ::RRSCore : public RCore
{
public:
    ~RRSCore() noexcept;
    RRSDevice *mainDevice() const noexcept { return (RRSDevice*)m_mainDevice; }
    void clearGarbage() noexcept override {};
private:
    friend class RCore;
    bool init() noexcept override;
    bool initDevices() noexcept;
    void unitDevices() noexcept;
    RRSCore(const Options &options) noexcept;
};

#endif // CZ_RRSCORE_H
