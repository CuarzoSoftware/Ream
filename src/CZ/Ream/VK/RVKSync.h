#ifndef CZ_RVKSYNC_H
#define CZ_RVKSYNC_H

#include <CZ/Ream/RSync.h>
#include <CZ/Ream/DRM/RDRMTimeline.h>
#include <CZ/Core/CZSpFd.h>
#include <vulkan/vulkan.h>
#include <memory>
#include <thread>

/**
 * @brief Vulkan RSync backed by a DRM syncobj timeline.
 *
 * NVIDIA's Vulkan does not support exporting/importing semaphores as @c sync_file (@c SYNC_FD) —
 * only @c OPAQUE_FD, which for timeline semaphores on Linux aliases a @c drm_syncobj. Minting the
 * fd straight from a @c SYNC_FD semaphore silently failed on NVIDIA and broke synchronization, so
 * the durable representation is a @c drm_syncobj timeline point instead:
 *
 *  - Make(): import the syncobj into a Vulkan timeline semaphore via @c OPAQUE_FD, then an empty
 *    submit signals it to the point after all prior queue work (the GPU signal materializes the
 *    syncobj point).
 *  - fd(): export a @c sync_file from the syncobj point (DRM ioctl — driver-agnostic, works on NVIDIA).
 *  - FromExternal(): import a @c sync_file into a fresh syncobj point (DRM ioctl).
 *
 * If the device lacks the Timeline capability, it degrades to a @c SYNC_FD semaphore (m_fd), or to
 * an already-signaled no-op if that is unavailable too.
 */
class CZ::RVKSync final : public RSync
{
public:
    /**
     * @brief Captures all queue work submitted so far on @p device as a sync.
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

    // Primary representation: a syncobj timeline point (NVIDIA-safe interop hub). Null on devices
    // without the Timeline capability, in which case m_fd is used instead.
    std::shared_ptr<RDRMTimeline> m_timeline;
    UInt64 m_point { 1 };

    // Fallback sync_file (used only when m_timeline is null). -1 means "already signaled".
    CZSpFd m_fd { -1 };

    // Make() only: the timeline semaphore aliasing the syncobj, kept alive until its signalling
    // submit completes (tracked by m_fence so no full device wait is needed).
    VkSemaphore m_semaphore { VK_NULL_HANDLE };
    VkFence m_fence { VK_NULL_HANDLE };

    // Thread that submitted this sync's work (Make only). A same-device waiter on this same thread
    // is already queue-ordered after it, so no explicit wait is needed (cf. RGLSync).
    std::thread::id m_threadId;
};

#endif // CZ_RVKSYNC_H
