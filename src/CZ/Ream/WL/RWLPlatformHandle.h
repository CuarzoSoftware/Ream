#ifndef RWLPLATFORMHANDLE_H
#define RWLPLATFORMHANDLE_H

#include <CZ/Ream/RPlatformHandle.h>
#include <memory>

struct wl_display;

class CZ::RWLPlatformHandle : public RPlatformHandle
{
public:
    static std::shared_ptr<RWLPlatformHandle> Make(wl_display *wlDisplay) noexcept;
    wl_display *wlDisplay() const noexcept { return m_wlDisplay; }

private:
    RWLPlatformHandle(wl_display *wlDisplay) noexcept :
        RPlatformHandle(RPlatform::Wayland),
        m_wlDisplay(wlDisplay) {}
    wl_display *m_wlDisplay { nullptr };
};

#endif // RWLPLATFORMHANDLE_H
