#ifndef RSYNC_H
#define RSYNC_H

#include <CZ/Ream/RObject.h>
#include <CZ/Core/CZSpFd.h>
#include <memory>

/**
 * @brief GPU/CPU synchronization fence.
 *
 * RSync represents a point in a device's command stream, backed by a @c sync_file fence. It lets
 * consumers wait for prior GPU work to complete before reading or overwriting a resource, without
 * stalling unnecessarily.
 *
 * A sync can be waited on the GPU (gpuWait(), inserting a dependency into a device's next
 * submission) or on the CPU (cpuWait()). Because it is expressed as a @c sync_file fd, it can be
 * exported and imported (fd(), RSync::FromExternal()) to interoperate with other devices, clients,
 * and DRM sync objects. Images publish their read/write syncs automatically as rendering proceeds.
 */
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
