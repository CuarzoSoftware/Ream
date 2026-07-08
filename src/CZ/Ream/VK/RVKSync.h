#ifndef CZ_RVKSYNC_H
#define CZ_RVKSYNC_H

#include <CZ/Ream/RSync.h>
#include <CZ/Core/CZSpFd.h>
#include <vulkan/vulkan.h>

/**
 * @brief Vulkan RSync represented by a sync_file fd.
 *
 * Like the GL backend's Android-native-fence approach, the durable representation is a
 * sync_file fd (poll()able and freely dup-able). A VkSemaphore is only used transiently to
 * mint the fd (Make: empty submit signals a SYNC_FD-exportable semaphore, then export) or to
 * consume it (gpuWait: import into the waiter's next submission). An fd of -1 means the sync
 * is already signaled.
 */
class CZ::RVKSync final : public RSync
{
public:
    /**
     * @brief Captures all queue work submitted so far on @p device as a sync.
     *
     * Performs an empty submit that signals a SYNC_FD-exportable semaphore, then exports it as a
     * sync_file fd. If SYNC_FD export is unavailable, returns an already-signaled (no-op) sync.
     */
    static std::shared_ptr<RVKSync> Make(RVKDevice *device) noexcept;

    /**
     * @brief Wraps an existing sync_file fd as a sync on @p device.
     *
     * Takes ownership of @p fd even on failure (per the RSync contract).
     */
    static std::shared_ptr<RVKSync> FromExternal(int fd, RVKDevice *device) noexcept;

    bool gpuWait(RDevice *waiter = nullptr) const noexcept override;
    int cpuWait(int timeoutMs = -1) const noexcept override;
    CZSpFd fd() const noexcept override;

    ~RVKSync() noexcept;
private:
    RVKSync(std::shared_ptr<RCore> core, RVKDevice *device, bool isExternal) noexcept;
    RVKDevice *dev() const noexcept { return (RVKDevice*)m_device; }

    // The sync_file. -1 means "already signaled" (a no-op sync).
    CZSpFd m_fd { -1 };

    // Kept alive until the signalling submit completes (Make only). The fence tracks completion
    // so the semaphore can be destroyed without a full device wait.
    VkSemaphore m_semaphore { VK_NULL_HANDLE };
    VkFence m_fence { VK_NULL_HANDLE };
};

#endif // CZ_RVKSYNC_H
