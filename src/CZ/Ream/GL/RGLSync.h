#ifndef RGLSYNC_H
#define RGLSYNC_H

#include <CZ/Ream/RSync.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <thread>

/**
 * @brief OpenGL backend implementation of RSync.
 *
 * Wraps an EGL fence sync object (`EGLSyncKHR`). It is either created for the current point in a
 * device's GL command stream (Make(), preferring a native-fence sync so it can be exported as a
 * `sync_file` fd) or imported from an external fence fd (FromExternal()). Waits are performed via
 * the device's EGL sync procs; cross-device GPU waits go through fd export/import.
 */
class CZ::RGLSync final : public RSync
{
public:
    /**
     * @brief Creates a sync by importing an external `sync_file` fence.
     *
     * Takes ownership of the fd even on failure. Requires the device's SyncImport capability.
     *
     * @param fd     The fence file descriptor to import.
     * @param device Target device, or `nullptr` for the main device.
     * @return The new sync, or `nullptr` on failure.
     */
    [[nodiscard]] static std::shared_ptr<RGLSync> FromExternal(int fd, RGLDevice *device = nullptr) noexcept;

    /**
     * @brief Creates a sync marking the current point in the device's GL command stream.
     *
     * Requires the device's SyncCPU capability; prefers a native-fence sync (exportable to an fd)
     * when SyncExport is available, otherwise falls back to a plain EGL fence.
     *
     * @param device Target device, or `nullptr` for the main device.
     * @return The new sync, or `nullptr` on failure.
     */
    [[nodiscard]] static std::shared_ptr<RGLSync> Make(RGLDevice *device = nullptr) noexcept;

    /**
     * @brief Destroys the underlying EGL sync object.
     */
    ~RGLSync() noexcept;

    /**
     * @brief Returns the device this sync belongs to as an RGLDevice.
     */
    RGLDevice *device() const noexcept { return (RGLDevice*)m_device; }
    bool gpuWait(RDevice *waiter = nullptr) const noexcept override;
    int cpuWait(int timeoutMs = -1) const noexcept override;
    CZSpFd fd() const noexcept override;

private:
    RGLSync(std::shared_ptr<RCore> core, RGLDevice *device, EGLSyncKHR sync, bool isExternal) noexcept;
    EGLSyncKHR m_eglSync;
    std::thread::id m_threadId;
};

#endif // RGLSYNC_H
