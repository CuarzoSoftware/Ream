#ifndef RWLPLATFORMHANDLE_H
#define RWLPLATFORMHANDLE_H

#include <CZ/Ream/RPlatformHandle.h>
#include <CZ/Core/CZOwn.h>
#include <sys/types.h>
#include <memory>

struct wl_display;

/**
 * @brief Wayland platform handle.
 *
 * RWLPlatformHandle is the RPlatformHandle for the Wayland platform (RPlatform::Wayland). It wraps
 * a client @c wl_display connection that Ream uses to talk to the compositor, and can be obtained
 * from an RPlatformHandle via asWL().
 */
class CZ::RWLPlatformHandle : public RPlatformHandle
{
public:
    /**
     * @brief Creates a Wayland platform handle from an existing display connection.
     *
     * @param wlDisplay The @c wl_display connection. Must not be null.
     * @param ownership Whether this handle owns @p wlDisplay (and disconnects it on destruction)
     *                  or merely borrows it.
     *
     * @return A valid handle on success, nullptr if @p wlDisplay is null.
     */
    static std::shared_ptr<RWLPlatformHandle> Make(wl_display *wlDisplay, CZOwn ownership) noexcept;

    /**
     * @brief Returns the wrapped @c wl_display connection.
     */
    wl_display *wlDisplay() const noexcept { return m_wlDisplay; }

    /**
     * @brief The compositor's advertised main device (from linux-dmabuf feedback).
     *
     * This is the DRM device the compositor uses to import/composite client buffers. Backends
     * (notably Vulkan) should prefer the render device matching this @c dev_t so buffers and
     * explicit-sync timelines stay on a single GPU. Returns 0 if the compositor did not advertise
     * one (e.g. no @c zwp_linux_dmabuf_v1 v4+ support).
     */
    dev_t mainDevice() const noexcept { return m_mainDevice; }

    /**
     * @brief Destroys the handle, disconnecting the @c wl_display if it is owned.
     */
    ~RWLPlatformHandle() noexcept;
private:
    RWLPlatformHandle(wl_display *wlDisplay, CZOwn ownership) noexcept :
        RPlatformHandle(RPlatform::Wayland),
        m_wlDisplay(wlDisplay),
        m_own(ownership) {}
    wl_display *m_wlDisplay { nullptr };
    CZOwn m_own;
    dev_t m_mainDevice { 0 };
};

#endif // RWLPLATFORMHANDLE_H
