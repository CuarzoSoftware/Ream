#include <CZ/Ream/RResourceTracker.h>
#include <CZ/Ream/RLog.h>

using namespace CZ;

struct ResourceTracker
{
    ~ResourceTracker() noexcept
    {
        RLog(CZTrace, "Memory leaks:");
        RLog(CZTrace, "- RCore: {}", count[RCoreRes]);
        RLog(CZTrace, "- RImage: {}", count[RImageRes]);
        RLog(CZTrace, "- RSurface: {}", count[RSurfaceRes]);
        RLog(CZTrace, "- RDevice: {}", count[RDeviceRes]);
    }
    int count[RResLast] {};
};

static ResourceTracker rt {};

void CZ::RResourceTrackerAdd(RResourceType type) noexcept
{
    rt.count[type]++;
}

void CZ::RResourceTrackerSub(RResourceType type) noexcept
{
    rt.count[type]--;
}

int CZ::RResourceTrackerGet(RResourceType type) noexcept
{
    return rt.count[type];
}
