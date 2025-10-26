#ifndef RSYNC_H
#define RSYNC_H

#include <CZ/Ream/RObject.h>
#include <CZ/Core/CZSpFd.h>
#include <memory>

class CZ::RSync : public RObject
{
public:
    // Takes ownership of the fd even on failure
    static std::shared_ptr<RSync> FromExternal(int fd, RDevice *device = nullptr) noexcept;

    static std::shared_ptr<RSync> Make(RDevice *device = nullptr) noexcept;

    // True on success or if already signaled, non-blocking
    virtual bool gpuWait(RDevice *waiter = nullptr) const noexcept = 0;

    // 1 if already signaled, -1 on error, 0 on timeout, blocking
    virtual int cpuWait(int timeoutMs = -1) const noexcept = 0;

    // Returns -1 on error
    virtual CZSpFd fd() const noexcept = 0;

    // True if created with FromExternal
    bool isExternal() const noexcept { return m_isExternal; }

    RDevice *device() const noexcept { return m_device; }

protected:
    RSync(std::shared_ptr<RCore> core, RDevice *device, bool isExternal) noexcept;
    ~RSync() noexcept;
    bool m_isExternal;
    RDevice *m_device;
    std::shared_ptr<RCore> m_core;
};

#endif // RSYNC_H
