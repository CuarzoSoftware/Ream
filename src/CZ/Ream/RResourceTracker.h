#ifndef CZ_RRESOURCETRACKER_H
#define CZ_RRESOURCETRACKER_H

namespace CZ
{
    /**
     * @brief Categories of Ream resources whose live instances are counted.
     *
     * Used by the resource tracker to detect leaks by keeping a per-type instance count.
     */
    enum RResourceType
    {
        RCoreRes,           ///< RCore instances.
        RImageRes,          ///< RImage instances.
        RSurfaceRes,        ///< RSurface instances.
        RSyncRes,           ///< RSync instances.
        RPainterRes,        ///< RPainter instances.
        RDeviceRes,         ///< RDevice instances.
        RDRMFramebufferRes, ///< RDRMFramebuffer instances.
        RDRMTimelineRes,    ///< RDRMTimeline instances.
        RDumbBufferRes,     ///< RDumbBuffer instances.
        RGBMBoRes,          ///< RGBMBo instances.
        REGLImageRes,       ///< REGLImage instances.
        RResLast            ///< Sentinel marking the number of resource types.
    };

    /**
     * @brief Increments the live-instance count for the given resource type.
     *
     * @param type The resource type to track.
     */
    void RResourceTrackerAdd(RResourceType type) noexcept;

    /**
     * @brief Decrements the live-instance count for the given resource type.
     *
     * @param type The resource type to track.
     */
    void RResourceTrackerSub(RResourceType type) noexcept;

    /**
     * @brief Returns the current live-instance count for the given resource type.
     *
     * @param type The resource type to query.
     * @return The number of live instances of that type.
     */
    int  RResourceTrackerGet(RResourceType type) noexcept;

    /**
     * @brief Logs the current live-instance count of every resource type.
     *
     * Intended for detecting leaks; output is emitted at trace verbosity.
     */
    void RResourceTrackerLog() noexcept;
};

#endif // CZ_RRESOURCETRACKER_H
