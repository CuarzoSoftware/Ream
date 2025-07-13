#include <CZ/Ream/WL/RWLPlatformHandle.h>
#include <CZ/Ream/RLog.h>

using namespace CZ;

std::shared_ptr<RWLPlatformHandle> RWLPlatformHandle::Make(wl_display *wlDisplay) noexcept
{
    if (!wlDisplay)
    {
        RLog(CZFatal, CZLN, "Invalid wl_display handle");
        return nullptr;
    }

    return std::shared_ptr<RWLPlatformHandle>(new RWLPlatformHandle(wlDisplay));
}
