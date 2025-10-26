#ifndef CZ_RRESOURCETRACKER_H
#define CZ_RRESOURCETRACKER_H

namespace CZ
{
    enum RResourceType
    {
        RCoreRes,
        RImageRes,
        RSurfaceRes,
        RSyncRes,
        RPainterRes,
        RDeviceRes,
        RDRMFramebufferRes,
        RDRMTimelineRes,
        RDumbBufferRes,
        RGBMBoRes,
        REGLImageRes,
        RResLast
    };

    void RResourceTrackerAdd(RResourceType type) noexcept;
    void RResourceTrackerSub(RResourceType type) noexcept;
    int  RResourceTrackerGet(RResourceType type) noexcept;
    void RResourceTrackerLog() noexcept;
};

#endif // CZ_RRESOURCETRACKER_H
