#include <CZ/Ream/VK/RVKSync.h>
#include <CZ/Ream/VK/RVKDevice.h>
#include <CZ/Ream/VK/RVKCore.h>
#include <CZ/Ream/RCore.h>
#include <CZ/Ream/RLog.h>

#include <poll.h>
#include <unistd.h>

using namespace CZ;

RVKSync::RVKSync(std::shared_ptr<RCore> core, RVKDevice *device, bool isExternal) noexcept :
    RSync(core, device, isExternal)
{}

RVKSync::~RVKSync() noexcept
{
    if (!m_device || dev()->device() == VK_NULL_HANDLE)
        return;

    const VkDevice d { dev()->device() };

    // Wait only for this sync's own submit (cheap if already done), then release its objects.
    if (m_fence != VK_NULL_HANDLE)
    {
        vkWaitForFences(d, 1, &m_fence, VK_TRUE, UINT64_MAX);
        vkDestroyFence(d, m_fence, nullptr);
    }
    if (m_semaphore != VK_NULL_HANDLE)
        vkDestroySemaphore(d, m_semaphore, nullptr);
}

// Transient SYNC_FD-exportable binary semaphore.
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

std::shared_ptr<RVKSync> RVKSync::Make(RVKDevice *device) noexcept
{
    auto core { RCore::Get() };
    if (!core || !device)
    {
        RLog(CZError, CZLN, "RVKSync::Make: missing core/device");
        return {};
    }

    auto sync { std::shared_ptr<RVKSync>(new RVKSync(core, device, false)) };

    // Without SYNC_FD export the sync degrades to an already-signaled no-op.
    if (!device->procs().getSemaphoreFdKHR)
        return sync;

    VkSemaphore sem { MakeExportableSemaphore(device) };
    if (sem == VK_NULL_HANDLE)
    {
        RLog(CZError, CZLN, "RVKSync::Make: vkCreateSemaphore failed");
        return {};
    }

    VkFenceCreateInfo fci {};
    fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkFence fence { VK_NULL_HANDLE };
    if (vkCreateFence(device->device(), &fci, nullptr, &fence) != VK_SUCCESS)
    {
        vkDestroySemaphore(device->device(), sem, nullptr);
        return {};
    }

    // Empty submit signaling the semaphore (and fence) after all prior queue work.
    if (!device->submitSignal(sem, fence))
    {
        RLog(CZError, CZLN, "RVKSync::Make: submitSignal failed");
        vkDestroyFence(device->device(), fence, nullptr);
        vkDestroySemaphore(device->device(), sem, nullptr);
        return {};
    }

    // Export the pending signal as a sync_file. Copy transference resets the semaphore's payload,
    // but the semaphore is retained only so it can be destroyed once the fence completes.
    // out == -1 means already signaled.
    VkSemaphoreGetFdInfoKHR gi {};
    gi.sType = VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR;
    gi.semaphore = sem;
    gi.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT;

    int out { -1 };
    if (device->procs().getSemaphoreFdKHR(device->device(), &gi, &out) != VK_SUCCESS)
    {
        RLog(CZWarning, CZLN, "RVKSync::Make: vkGetSemaphoreFdKHR failed");
        out = -1;
    }

    sync->m_fd.reset(out);
    sync->m_semaphore = sem;
    sync->m_fence = fence;
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
    sync->m_fd.reset(owned.release()); // the sync_file fd is the sync
    return sync;
}

CZSpFd RVKSync::fd() const noexcept
{
    if (m_fd.get() < 0)
        return CZSpFd { -1 };
    return CZSpFd { fcntl(m_fd.get(), F_DUPFD_CLOEXEC, 0) };
}

int RVKSync::cpuWait(int timeoutMs) const noexcept
{
    if (m_fd.get() < 0)
        return 1; // already signaled

    pollfd p {};
    p.fd = m_fd.get();
    p.events = POLLIN;

    const int r { poll(&p, 1, timeoutMs) };
    if (r < 0)
        return -1;
    if (r == 0)
        return 0; // timeout
    return 1;
}

bool RVKSync::gpuWait(RDevice *waiter) const noexcept
{
    if (m_fd.get() < 0)
        return true; // already signaled

    RVKDevice *w { waiter ? waiter->asVK() : dev() };
    if (!w || !w->procs().importSemaphoreFdKHR)
        return false;

    const int dupFd { fcntl(m_fd.get(), F_DUPFD_CLOEXEC, 0) };
    if (dupFd < 0)
        return false;

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
        close(dupFd);
        vkDestroySemaphore(w->device(), temp, nullptr);
        return false;
    }

    // The next submission on the waiter device waits on (and then destroys) this semaphore.
    w->queueWait(temp, true);
    return true;
}
