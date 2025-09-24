#include <CZ/Ream/WL/RWLPlatformHandle.h>
#include <CZ/Ream/RLog.h>
#include <wayland-client-core.h>

using namespace CZ;

std::shared_ptr<RWLPlatformHandle> RWLPlatformHandle::Make(wl_display *wlDisplay, CZOwn ownership) noexcept
{
    if (!wlDisplay)
    {
        RLog(CZFatal, CZLN, "Invalid wl_display handle");
        return nullptr;
    }

    return std::shared_ptr<RWLPlatformHandle>(new RWLPlatformHandle(wlDisplay, ownership));
}

RWLPlatformHandle::~RWLPlatformHandle() noexcept
{
    if (m_wlDisplay && m_own == CZOwn::Own)
        wl_display_disconnect(m_wlDisplay);
}
