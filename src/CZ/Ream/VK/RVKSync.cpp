#include <CZ/Ream/VK/RVKSync.h>
#include <CZ/Ream/VK/RVKDevice.h>
#include <CZ/Ream/VK/RVKCore.h>
#include <CZ/Ream/RCore.h>
#include <CZ/Ream/RLog.h>

#include <poll.h>
#include <xf86drm.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdint>

using namespace CZ;

RVKSync::RVKSync(std::shared_ptr<RCore> core, RVKDevice *device, bool isExternal) noexcept :
    RSync(core, device, isExternal)
{}

RVKSync::~RVKSync() noexcept
{
    if (!m_device || dev()->device() == VK_NULL_HANDLE)
        return;

    const VkDevice d { dev()->device() };

    // Wait only for this sync's own signalling submit (cheap if already done), then release.
    if (m_fence != VK_NULL_HANDLE)
    {
        vkWaitForFences(d, 1, &m_fence, VK_TRUE, UINT64_MAX);
        vkDestroyFence(d, m_fence, nullptr);
    }
    if (m_semaphore != VK_NULL_HANDLE)
        vkDestroySemaphore(d, m_semaphore, nullptr);
}

// Transient SYNC_FD-exportable binary semaphore (used by gpuWait to import a sync_file into a
// waiter's next submission). Only used on devices that support SYNC_FD import (e.g. Mesa).
static VkSemaphore MakeExportableSemaphore(RVKDevice *device) noexcept
{
    VkExportSemaphoreCreateInfo exportInfo {};
    exportInfo.sType = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO;
    exportInfo.handleTypes = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT;

    VkSemaphoreCreateInfo ci {};
    ci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    ci.pNext = &exportInfo;

    VkSemaphore sem { VK_NULL_HANDLE };
    if (vkCreateSemaphore(device->device(), &ci, nullptr, &sem) != VK_SUCCESS)
        return VK_NULL_HANDLE;
    return sem;
}

// Creates a Vulkan TIMELINE semaphore and imports the given drm_syncobj fd via OPAQUE_FD, so that
// signalling the semaphore signals the syncobj timeline. On success Vulkan owns @p syncobjFd.
static VkSemaphore ImportSyncobjTimeline(RVKDevice *device, int syncobjFd) noexcept
{
    if (!device->procs().importSemaphoreFdKHR)
        return VK_NULL_HANDLE;

    VkSemaphoreTypeCreateInfo tci {};
    tci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
    tci.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    tci.initialValue = 0;

    VkSemaphoreCreateInfo ci {};
    ci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    ci.pNext = &tci;

    VkSemaphore sem { VK_NULL_HANDLE };
    if (vkCreateSemaphore(device->device(), &ci, nullptr, &sem) != VK_SUCCESS)
        return VK_NULL_HANDLE;

    VkImportSemaphoreFdInfoKHR imp {};
    imp.sType = VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_FD_INFO_KHR;
    imp.semaphore = sem;
    imp.flags = 0; // permanent import
    imp.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;
    imp.fd = syncobjFd; // Vulkan takes ownership on success

    if (device->procs().importSemaphoreFdKHR(device->device(), &imp) != VK_SUCCESS)
    {
        vkDestroySemaphore(device->device(), sem, nullptr);
        return VK_NULL_HANDLE; // fd NOT consumed on failure
    }
    return sem;
}

bool RVKDevice::initSyncTimeline() const noexcept
{
    std::lock_guard<std::mutex> lock { m_queueMutex };

    if (m_syncTimelineState < 0)
        return false;

    if (m_syncTimelineState == 0)
    {
        m_syncTimelineState = -1;
        auto tl { RDRMTimeline::Make(const_cast<RVKDevice*>(this)) };

        if (!tl)
            return false;

        CZSpFd soFd { tl->exportTimeline() };

        if (soFd.get() < 0)
            return false;

        const VkSemaphore sem { ImportSyncobjTimeline(const_cast<RVKDevice*>(this), soFd.get()) };

        if (sem == VK_NULL_HANDLE)
            return false; // e.g. NVIDIA: each sync falls back to its own fence

        (void)soFd.release(); // Consumed by the import
        m_syncTimeline = tl;
        m_syncSemaphore = sem;
        m_syncTimelineState = 1;
    }

    return true;
}

bool RVKDevice::signalSyncTimeline(std::shared_ptr<RDRMTimeline> &timeline, UInt64 &point, std::shared_ptr<SyncPoint> &pending) const noexcept
{
    if (!initSyncTimeline())
        return false;

    // This thread's stream is open: the sync covers it, its value is assigned when it is queued
    if (Stream *s { findStream(std::this_thread::get_id(), false) })
    {
        const auto attach = [&]() -> bool
        {
            if (s->cmd.pool == VK_NULL_HANDLE)
                return false;

            if (!s->point)
                s->point = std::make_shared<SyncPoint>();

            pending = s->point;
            return true;
        };

        bool attached;

        if (s->recording) // This thread holds its mutex
            attached = attach();
        else
        {
            std::lock_guard<std::mutex> lock { s->mutex };
            attached = attach();
        }

        if (attached)
        {
            std::lock_guard<std::mutex> lock { m_queueMutex };
            timeline = m_syncTimeline;
            point = 0;
            return true;
        }
    }

    std::lock_guard<std::mutex> lock { m_queueMutex };

    // After all the work submitted so far (same queue)
    const UInt64 value { ++m_syncPoint };

    // Commands are waiting to be sent: signalled by that submission (see flushBatchLocked()).
    // Whoever exports or CPU-waits this point sends the batch first.
    if (!m_batchCmds.empty())
    {
        m_batchSignalsSync = true;
        timeline = m_syncTimeline;
        point = value;
        return true;
    }

    VkTimelineSemaphoreSubmitInfo tsi {};
    tsi.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
    tsi.signalSemaphoreValueCount = 1;
    tsi.pSignalSemaphoreValues = &value;

    VkSubmitInfo submit {};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.pNext = &tsi;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &m_syncSemaphore;

    if (vkQueueSubmit(m_graphicsQueue, 1, &submit, VK_NULL_HANDLE) != VK_SUCCESS)
        return false;

    timeline = m_syncTimeline;
    point = value;
    return true;
}

std::shared_ptr<RVKSync> RVKSync::Make(RVKDevice *device) noexcept
{
    auto core { RCore::Get() };
    if (!core || !device)
    {
        RLog(CZError, CZLN, "RVKSync::Make: missing core/device");
        return {};
    }

    auto sync { std::shared_ptr<RVKSync>(new RVKSync(core, device, false)) };

    // A point of the device's shared timeline (cheap: no syncobj, import or fence per sync)
    if (device->caps().Timeline)
    {
        std::shared_ptr<RDRMTimeline> tl;
        UInt64 point;

        std::shared_ptr<RVKDevice::SyncPoint> pending;

        if (device->signalSyncTimeline(tl, point, pending))
        {
            sync->m_timeline = tl;
            sync->m_point = point;
            sync->m_pendingPoint = std::move(pending);
            sync->m_sharedTimeline = true;
            return sync;
        }
    }

    // Preferred (NVIDIA-safe) path: a drm_syncobj timeline point signalled by the GPU via a
    // timeline semaphore imported from the syncobj (OPAQUE_FD). The sync_file is later exported
    // from the syncobj via a DRM ioctl.
    // Fast path (Mesa): alias a drm_syncobj timeline into a Vulkan timeline semaphore via OPAQUE_FD,
    // signal it on the GPU, and export a sync_file from the syncobj. Not supported on NVIDIA (it
    // rejects importing an external drm_syncobj as an OPAQUE_FD semaphore).
    if (device->caps().Timeline)
    {
        auto tl { RDRMTimeline::Make(device) };
        if (tl)
        {
            CZSpFd soFd { tl->exportTimeline() };
            if (soFd.get() >= 0)
            {
                VkSemaphore sem { ImportSyncobjTimeline(device, soFd.get()) };
                if (sem != VK_NULL_HANDLE)
                {
                    (void)soFd.release(); // consumed by the import

                    VkFence fence { VK_NULL_HANDLE };
                    VkFenceCreateInfo fci {};
                    fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
                    if (vkCreateFence(device->device(), &fci, nullptr, &fence) == VK_SUCCESS
                        && device->submitSignalTimeline(sem, 1, fence))
                    {
                        sync->m_timeline = tl;
                        sync->m_point = 1;
                        sync->m_semaphore = sem;
                        sync->m_fence = fence;
                        return sync;
                    }

                    if (fence != VK_NULL_HANDLE) vkDestroyFence(device->device(), fence, nullptr);
                    vkDestroySemaphore(device->device(), sem, nullptr);
                }
            }
        }
    }

    // Fallback (NVIDIA): no sync_file interop available. Track completion with a plain fence so
    // cpuWait() and gpuWait() wait correctly on the CPU; fd() returns -1 (consumers that need a
    // sync_file, e.g. the KMS in-fence, fall back to a device CPU wait). This is correct, just not
    // zero-copy.
    {
        VkFence fence { VK_NULL_HANDLE };
        VkFenceCreateInfo fci {};
        fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        if (vkCreateFence(device->device(), &fci, nullptr, &fence) == VK_SUCCESS
            && device->submitSignal(VK_NULL_HANDLE, fence))
            sync->m_fence = fence;
        else if (fence != VK_NULL_HANDLE)
            vkDestroyFence(device->device(), fence, nullptr);
    }

    return sync;
}

std::shared_ptr<RVKSync> RVKSync::FromExternal(int fd, RVKDevice *device) noexcept
{
    CZSpFd owned { fd }; // takes ownership even on failure (per RSync contract)

    auto core { RCore::Get() };
    if (!core || !device)
    {
        RLog(CZError, CZLN, "RVKSync::FromExternal: missing core/device");
        return {};
    }

    auto sync { std::shared_ptr<RVKSync>(new RVKSync(core, device, true)) };

    // Import the sync_file into a fresh syncobj point (DRM ioctl, driver-agnostic) so it can be
    // re-exported later even on NVIDIA. Falls back to keeping the raw sync_file otherwise.
    if (device->caps().Timeline && owned.get() >= 0)
    {
        auto tl { RDRMTimeline::Make(device) };
        if (tl && tl->importSyncFile(owned.get(), 1, CZOwn::Borrow))
        {
            sync->m_timeline = tl;
            sync->m_point = 1;
            return sync; // owned closed below
        }
    }

    sync->m_fd.reset(owned.release()); // the sync_file fd is the sync
    return sync;
}

UInt64 RVKSync::point() const noexcept
{
    if (!m_pendingPoint)
        return m_point;

    return dev()->resolvePoint(m_pendingPoint, threadId());
}

CZSpFd RVKSync::fd() const noexcept
{
    // A point of the shared timeline may still be in the device's pending batch
    if (m_sharedTimeline)
        dev()->flushBatch();

    if (m_timeline)
    {
        const UInt64 p { point() };
        return p ? m_timeline->exportSyncFile(p) : CZSpFd { -1 };
    }

    if (m_fd.get() < 0)
        return CZSpFd { -1 };
    return CZSpFd { fcntl(m_fd.get(), F_DUPFD_CLOEXEC, 0) };
}

int RVKSync::cpuWait(int timeoutMs) const noexcept
{
    if (m_signaled.load(std::memory_order_acquire))
        return 1;

    const int ret { cpuWaitImpl(timeoutMs) };

    if (ret == 1)
        m_signaled.store(true, std::memory_order_release);

    return ret;
}

int RVKSync::cpuWaitImpl(int timeoutMs) const noexcept
{
    // Make() path: the fence tracks the signalling submit directly.
    if (m_fence != VK_NULL_HANDLE)
    {
        const uint64_t ns { timeoutMs < 0 ? UINT64_MAX : (uint64_t)timeoutMs * 1000000ull };
        const VkResult r { vkWaitForFences(dev()->device(), 1, &m_fence, VK_TRUE, ns) };
        if (r == VK_SUCCESS) return 1;
        if (r == VK_TIMEOUT) return 0;
        return -1;
    }

    // Imported syncobj (FromExternal), or a point of the shared timeline (Make(): its signal may
    // not have reached the kernel yet, hence WAIT_FOR_SUBMIT): wait on the timeline point.
    if (m_timeline)
    {
        if (m_sharedTimeline)
            dev()->flushBatch();

        const UInt64 p { point() };

        if (!p)
            return 0; // Its thread is still recording it

        const Int64 ns { timeoutMs < 0 ? INT64_MAX : (Int64)timeoutMs * 1000000ll };
        return m_timeline->waitSync(p, m_sharedTimeline ? DRM_SYNCOBJ_WAIT_FLAGS_WAIT_FOR_SUBMIT : 0, ns);
    }

    if (m_fd.get() < 0)
        return 1; // already signaled

    pollfd p {};
    p.fd = m_fd.get();
    p.events = POLLIN;
    const int r { poll(&p, 1, timeoutMs) };
    if (r < 0) return -1;
    if (r == 0) return 0;
    return 1;
}

bool RVKSync::gpuWait(RDevice *waiter) const noexcept
{
    RVKDevice *w { waiter ? waiter->asVK() : dev() };
    if (!w)
        return false;

    // Same device AND same thread that submitted this sync's work: that thread's later submissions
    // are already queue-ordered after it (like GL's within-context/thread ordering), so no wait is
    // needed. Cross-thread submissions to the same queue interleave non-deterministically, so they
    // must still wait. Not valid for imported (external) syncs, whose work is not on this queue.
    if (!isExternal() && w == dev() && std::this_thread::get_id() == m_threadId)
        return true;

    if (m_signaled.load(std::memory_order_acquire))
        return true;

    // A client buffer's acquire sync is usually signalled long before it is drawn, and drawn many
    // times (each frame, each pass): a poll is one ioctl, exporting and importing it many more.
    // Not for this device's own syncs (a poll would send its pending batch).
    if (isExternal() && cpuWait(0) == 1)
        return true;

    {
        std::lock_guard<std::mutex> lock { m_queuedMutex };
        if (m_queuedOn == w && m_queuedThread == std::this_thread::get_id() && m_queuedSerial == w->waitsSerial())
            return true; // The waiter's next submission waits for it already
    }

    if (!w->procs().importSemaphoreFdKHR)
        return false;

    // Obtain a sync_file for this sync's completion (exported from the syncobj, or the raw fd).
    auto fence { fd() };
    if (fence.get() < 0)
    {
        // No sync_file available (NVIDIA fallback). If there is real pending work tracked by a
        // fence, block on the CPU so the waiter's subsequent work is correctly ordered after it;
        // otherwise there is nothing to wait on.
        if (m_fence != VK_NULL_HANDLE)
            cpuWait();
        return true;
    }

    const int dupFd { fence.release() }; // Ours (exported or duplicated by fd())

    VkSemaphore temp { MakeExportableSemaphore(w) };
    if (temp == VK_NULL_HANDLE)
    {
        close(dupFd);
        return false;
    }

    VkImportSemaphoreFdInfoKHR imp {};
    imp.sType = VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_FD_INFO_KHR;
    imp.semaphore = temp;
    imp.flags = VK_SEMAPHORE_IMPORT_TEMPORARY_BIT;
    imp.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT;
    imp.fd = dupFd; // Vulkan takes ownership on success

    if (w->procs().importSemaphoreFdKHR(w->device(), &imp) != VK_SUCCESS)
    {
        // SYNC_FD import unsupported on this waiter (e.g. NVIDIA). Caller must fall back to a CPU
        // wait (SRM does reamDevice()->wait() for such devices).
        close(dupFd);
        vkDestroySemaphore(w->device(), temp, nullptr);
        return false;
    }

    // The next submission on the waiter device waits on (and then destroys) this semaphore.
    const UInt64 serial { w->queueWait(temp, true) };
    std::lock_guard<std::mutex> lock { m_queuedMutex };
    m_queuedOn = w;
    m_queuedThread = std::this_thread::get_id();
    m_queuedSerial = serial;
    return true;
}
