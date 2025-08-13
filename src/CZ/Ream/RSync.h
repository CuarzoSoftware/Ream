#ifndef RSYNC_H
#define RSYNC_H

#include <CZ/Ream/RObject.h>
#include <CZ/CZSpFd.h>
#include <memory>

class CZ::RSync : public RObject
{
public:
    // Takes ownership of the fd even on failure
    static std::shared_ptr<RSync> FromExternal(int fd, RDevice *device = nullptr) noexcept;

    static std::shared_ptr<RSync> Make(RDevice *device = nullptr) noexcept;

    // True on success or if already signaled, non-blocking
    virtual bool gpuWait(RDevice *waiter = nullptr) const noexcept = 0;

    // True on success or if already signaled, false on error or timeout, blocking
    virtual bool cpuWait(int timeoutMs = -1) const noexcept = 0;

    // Returns a dup fd or -1 on error
    CZSpFd fd() const noexcept { return m_fd.dup(); }

    // True if created with FromExternal
    bool isExternal() const noexcept { return m_isExternal; }

    RDevice *device() const noexcept { return m_device; }

protected:
    RSync(std::shared_ptr<RCore> core, RDevice *device, int fd, bool isExternal) noexcept;
    ~RSync() noexcept;
    bool m_isExternal;
    CZSpFd m_fd;
    RDevice *m_device;
    std::shared_ptr<RCore> m_core;
};

#endif // RSYNC_H
