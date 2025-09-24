#ifndef RWLPLATFORMHANDLE_H
#define RWLPLATFORMHANDLE_H

#include <CZ/Ream/RPlatformHandle.h>
#include <CZ/Core/CZOwn.h>
#include <memory>

struct wl_display;

class CZ::RWLPlatformHandle : public RPlatformHandle
{
public:
    static std::shared_ptr<RWLPlatformHandle> Make(wl_display *wlDisplay, CZOwn ownership) noexcept;
    wl_display *wlDisplay() const noexcept { return m_wlDisplay; }
    ~RWLPlatformHandle() noexcept;
private:
    RWLPlatformHandle(wl_display *wlDisplay, CZOwn ownership) noexcept :
        RPlatformHandle(RPlatform::Wayland),
        m_wlDisplay(wlDisplay),
        m_own(ownership) {}
    wl_display *m_wlDisplay { nullptr };
    CZOwn m_own;
};

#endif // RWLPLATFORMHANDLE_H
