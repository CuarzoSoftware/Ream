#include <CZ/Ream/GL/RGLSync.h>
#include <CZ/Ream/GL/RGLDevice.h>
#include <CZ/Ream/GL/RGLCore.h>
#include <CZ/Ream/GL/RGLMakeCurrent.h>
#include <CZ/Ream/RLog.h>
#include <CZ/Ream/RLockGuard.h>
#include <fcntl.h>

using namespace CZ;

std::shared_ptr<RGLSync> RGLSync::Make(RGLDevice *device) noexcept
{
    RLockGuard lock {};

    auto core { RCore::Get() };

    if (!core)
    {
        RLog(CZError, CZLN, "Missing RCore");
        return {};
    }

    auto glCore { core->asGL() };

    if (!glCore)
    {
        RLog(CZError, CZLN, "Not an RGLCore");
        return {};
    }

    if (!device)
        device = glCore->mainDevice();

    if (!device->caps().SyncCPU)
        return {};

    auto curr { RGLMakeCurrent::FromDevice(device, true) };

    EGLSyncKHR sync { EGL_NO_SYNC_KHR };

    if (device->caps().SyncExport)
        sync = device->eglDisplayProcs().eglCreateSyncKHR(device->eglDisplay(), EGL_SYNC_NATIVE_FENCE_ANDROID, nullptr);

    if (sync == EGL_NO_SYNC_KHR)
    {
        sync = device->eglDisplayProcs().eglCreateSyncKHR(device->eglDisplay(), EGL_SYNC_FENCE_KHR, nullptr);

        if (sync == EGL_NO_SYNC_KHR)
        {
            device->log(CZError, CZLN, "Failed to create EGLSync");
            return {};
        }
    }

    return std::shared_ptr<RGLSync>(new RGLSync(core, device, sync, false));
}

std::shared_ptr<RGLSync> RGLSync::FromExternal(int fd, RGLDevice *device) noexcept
{
    RLockGuard lock {};

    CZSpFd fence { fd };

    if (fd < 0)
    {
        RLog(CZError, CZLN, "Failed to create RGLSync from external fence (invalid fd {}", fd);
        return {};
    }

    auto core { RCore::Get() };

    if (!core)
    {
        RLog(CZError, CZLN, "Missing RCore");
        return {};
    }

    auto glCore { core->asGL() };

    if (!glCore)
    {
        RLog(CZError, CZLN, "Not an RGLCore");
        return {};
    }

    if (!device)
        device = glCore->mainDevice();

    if (!device->caps().SyncImport)
    {
        device->log(CZDebug, "Failed to create RGLSync from external fence (missing SyncImport cap)");
        return {};
    }

    const EGLint attribs[3]
    {
        EGL_SYNC_NATIVE_FENCE_FD_ANDROID,
        fence.release(),
        EGL_NONE
    };

    auto curr { RGLMakeCurrent::FromDevice(device, true) };

    // eglCreateSyncKHR takes ownership even on failure
    EGLSyncKHR sync { device->eglDisplayProcs().eglCreateSyncKHR(device->eglDisplay(), EGL_SYNC_NATIVE_FENCE_ANDROID, attribs) };

    if (sync == EGL_NO_SYNC_KHR)
    {
        device->log(CZError, CZLN, "Failed to create RGLSync from external fence (eglCreateSyncKHR failed)");
        return {};
    }

    return std::shared_ptr<RGLSync>(new RGLSync(core, device, sync, true));
}

RGLSync::RGLSync(std::shared_ptr<RCore> core, RGLDevice *device, EGLSyncKHR sync, bool isExternal) noexcept :
    RSync(core, device, isExternal),
    m_eglSync(sync),
    m_threadId(std::this_thread::get_id())
{
    assert(sync != EGL_NO_SYNC_KHR);
}

RGLSync::~RGLSync() noexcept
{
    device()->eglDisplayProcs().eglDestroySyncKHR(device()->eglDisplay(), m_eglSync);
}

bool RGLSync::gpuWait(RDevice *waiter) const noexcept
{
    RLockGuard lock {};

    if (!waiter)
        waiter = m_core->mainDevice();

    if (!waiter->caps().SyncGPU)
        return false;

    if (device() == waiter)
    {
        // OpenGL guarantees command ordering within the same context and thread
        if (!isExternal() && std::this_thread::get_id() == m_threadId)
            return true;
        else
        {
            auto curr { RGLMakeCurrent::FromDevice(device(), true) };
            return device()->eglDisplayProcs().eglWaitSyncKHR(device()->eglDisplay(), m_eglSync, 0) == EGL_TRUE;
        }
    }

    if (!device()->caps().SyncExport || !waiter->caps().SyncImport)
        return false;

    auto fence { fd() };

    if (fence.get() < 0) return false;

    auto sync { RGLSync::FromExternal(fence.release(), (RGLDevice*)waiter) };

    if (!sync)
    {
        waiter->log(CZError, CZLN, "Failed to create RGLSync from external fence");
        return false;
    }

    return sync->gpuWait(waiter);
}

int RGLSync::cpuWait(int timeoutMs) const noexcept
{
    RLockGuard lock {};

    auto curr { RGLMakeCurrent::FromDevice(device(), true) };

    if (device()->eglDisplayProcs().eglClientWaitSyncKHR)
    {
        const auto ret = device()->eglDisplayProcs().eglClientWaitSyncKHR(
            device()->eglDisplay(),
            m_eglSync,
            0,
            timeoutMs < 0 ? EGL_FOREVER_KHR : timeoutMs * 1000000);

        if (ret == EGL_CONDITION_SATISFIED_KHR)
            return 1;
        else if (ret == EGL_TIMEOUT_EXPIRED)
            return 0;
    }

    return -1;
}

CZSpFd RGLSync::fd() const noexcept
{
    if (device()->eglDisplayProcs().eglDupNativeFenceFDANDROID)
        return CZSpFd(device()->eglDisplayProcs().eglDupNativeFenceFDANDROID(device()->eglDisplay(), m_eglSync));

    return CZSpFd();
}
