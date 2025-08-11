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

    int dupFd { -1 };
    EGLSyncKHR sync { EGL_NO_SYNC_KHR };

    if (device->caps().SyncExport)
        sync = device->eglDisplayProcs().eglCreateSyncKHR(device->eglDisplay(), EGL_SYNC_NATIVE_FENCE_ANDROID, nullptr);

    if (sync != EGL_NO_SYNC_KHR)
    {
        glFlush();

        if (device->eglDisplayProcs().eglDupNativeFenceFDANDROID)
            dupFd = device->eglDisplayProcs().eglDupNativeFenceFDANDROID(device->eglDisplay(), sync);
    }
    else
    {
        sync = device->eglDisplayProcs().eglCreateSyncKHR(device->eglDisplay(), EGL_SYNC_FENCE_KHR, nullptr);

        if (sync == EGL_NO_SYNC_KHR)
        {
            device->log(CZError, CZLN, "Failed to create EGLSync");
            return {};
        }
    }

    return std::shared_ptr<RGLSync>(new RGLSync(core, device, sync, dupFd, false));
}

std::shared_ptr<RGLSync> RGLSync::FromExternal(int fd, RGLDevice *device) noexcept
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

    if (fd < 0)
        return {};

    if (!device->caps().SyncImport)
    {
        close(fd);
        return {};
    }

    const EGLint attribs[3]
    {
        EGL_SYNC_NATIVE_FENCE_FD_ANDROID,
        fd,
        EGL_NONE
    };

    auto curr { RGLMakeCurrent::FromDevice(device, true) };
    EGLSyncKHR sync { device->eglDisplayProcs().eglCreateSyncKHR(device->eglDisplay(), EGL_SYNC_NATIVE_FENCE_ANDROID, attribs) };

    if (sync == EGL_NO_SYNC_KHR)
    {
        close(fd);
        device->log(CZError, CZLN, "Failed to create EGLSync");
        return {};
    }

    return std::shared_ptr<RGLSync>(new RGLSync(core, device, sync, -1, true));
}

RGLSync::RGLSync(std::shared_ptr<RCore> core, RGLDevice *device, EGLSyncKHR sync, int fd, bool isExternal) noexcept :
    RSync(core, device, fd, isExternal),
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

    const int fence { fd() };

    if (fence < 0) return false;

    auto sync { RGLSync::FromExternal(fence, (RGLDevice*)waiter) };

    if (!sync)
    {
        waiter->log(CZError, CZLN, "Failed to create RGLSync from external fence (fd: {})", fence);
        return false;
    }

    return sync->gpuWait(waiter);
}

bool RGLSync::cpuWait(int timeoutMs) const noexcept
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

        return ret == EGL_CONDITION_SATISFIED_KHR;
    }

    return false;
}
