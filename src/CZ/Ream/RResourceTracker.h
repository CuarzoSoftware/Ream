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
        RResLast
    };

    void RResourceTrackerAdd(RResourceType type) noexcept;
    void RResourceTrackerSub(RResourceType type) noexcept;
    int  RResourceTrackerGet(RResourceType type) noexcept;
};

#endif // CZ_RRESOURCETRACKER_H
