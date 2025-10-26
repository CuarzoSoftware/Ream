#include <CZ/Ream/RResourceTracker.h>
#include <CZ/Ream/RLog.h>

using namespace CZ;

struct ResourceTracker
{
    ~ResourceTracker() noexcept
    {
        log();
    }

    void log() noexcept
    {
        if (lvl >= 6) // The tracker is created before RLog
        {
            RLog(CZTrace, "Memory leaks:");
            RLog(CZTrace, "- RCore: {}", count[RCoreRes]);
            RLog(CZTrace, "- RImage: {}", count[RImageRes]);
            RLog(CZTrace, "- RSurface: {}", count[RSurfaceRes]);
            RLog(CZTrace, "- RDevice: {}", count[RDeviceRes]);
            RLog(CZTrace, "- RSync: {}", count[RSyncRes]);
            RLog(CZTrace, "- RDRMFramebuffer: {}", count[RDRMFramebufferRes]);
            RLog(CZTrace, "- RDRMTimeline: {}", count[RDRMTimelineRes]);
            RLog(CZTrace, "- RDumbBuffer: {}", count[RDumbBufferRes]);
            RLog(CZTrace, "- RGBMBo: {}", count[RGBMBoRes]);
            RLog(CZTrace, "- REGLImage: {}", count[REGLImageRes]);
        }
    }
    int count[RResLast] {};
    int lvl { 0 };
};

static ResourceTracker rt {};

void CZ::RResourceTrackerAdd(RResourceType type) noexcept
{
    rt.lvl = RLog.level();
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

void CZ::RResourceTrackerLog() noexcept
{
    rt.log();
}
