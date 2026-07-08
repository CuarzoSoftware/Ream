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
    /**
     * @brief Imports an external @c sync_file fence.
     *
     * @param fd     A @c sync_file file descriptor. Ownership is taken even on failure.
     * @param device Device to associate the sync with. If nullptr, the main device is used.
     * @return A shared pointer to the new RSync, or nullptr on failure.
     */
    static std::shared_ptr<RSync> FromExternal(int fd, RDevice *device = nullptr) noexcept;

    /**
     * @brief Creates a new sync fence.
     *
     * @param device Device to create the sync on. If nullptr, the main device is used.
     * @return A shared pointer to the new RSync, or nullptr on failure.
     */
    static std::shared_ptr<RSync> Make(RDevice *device = nullptr) noexcept;

    /**
     * @brief Inserts a GPU-side wait on this fence.
     *
     * Non-blocking. Makes the waiter's next submission depend on this fence being signaled.
     *
     * @param waiter Device that must wait. If nullptr, the main device is used.
     * @return true on success or if the fence is already signaled, false otherwise.
     */
    virtual bool gpuWait(RDevice *waiter = nullptr) const noexcept = 0;

    /**
     * @brief Blocks the calling thread until the fence is signaled.
     *
     * @param timeoutMs Timeout in milliseconds; -1 waits indefinitely.
     * @return 1 if signaled (or already signaled), 0 on timeout, -1 on error.
     */
    virtual int cpuWait(int timeoutMs = -1) const noexcept = 0;

    /**
     * @brief Exports this fence as a @c sync_file file descriptor.
     *
     * @return A file descriptor wrapper, invalid (-1) on error.
     */
    virtual CZSpFd fd() const noexcept = 0;

    /**
     * @brief Whether this sync was imported via FromExternal().
     *
     * @return true if created with FromExternal(), false otherwise.
     */
    bool isExternal() const noexcept { return m_isExternal; }

    /**
     * @brief Returns the device this sync is associated with.
     */
    RDevice *device() const noexcept { return m_device; }

protected:
    RSync(std::shared_ptr<RCore> core, RDevice *device, bool isExternal) noexcept;
    ~RSync() noexcept;
    bool m_isExternal;
    RDevice *m_device;
    std::shared_ptr<RCore> m_core;
};

#endif // RSYNC_H
