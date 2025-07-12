#ifndef RGLSYNC_H
#define RGLSYNC_H

#include <CZ/Ream/RSync.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <thread>

class CZ::RGLSync final : public RSync
{
public:
    [[nodiscard]] static std::shared_ptr<RGLSync> FromExternal(int fd, RGLDevice *device = nullptr) noexcept;
    [[nodiscard]] static std::shared_ptr<RGLSync> Make(RGLDevice *device = nullptr) noexcept;

    ~RGLSync() noexcept;
    RGLDevice *device() const noexcept { return (RGLDevice*)m_device; }
    bool gpuWait(RDevice *waiter = nullptr) const noexcept override;
    bool cpuWait(int timeoutMs = -1) const noexcept override;
private:
    RGLSync(std::shared_ptr<RCore> core, RGLDevice *device, EGLSyncKHR sync, int fd, bool isExternal) noexcept;
    EGLSyncKHR m_eglSync;
    std::thread::id m_threadId;
};

#endif // RGLSYNC_H
