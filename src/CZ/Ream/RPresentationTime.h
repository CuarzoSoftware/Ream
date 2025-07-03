#ifndef RPRESENTATIONTIME_H
#define RPRESENTATIONTIME_H

#include <CZ/Ream/Ream.h>
#include <CZ/CZBitset.h>
#include <ctime>

/**
 * @brief Struct representing presentation time information.
 *
 * This struct provides information about how the last framebuffer was presented on a connector display.
 */
struct CZ::RPresentationTime
{
    /**
     * @brief Bitmask of flags in an fb presentation.
     *
     * These flags provide information about how the last framebuffer was presented on a connector display.
     */
    enum Flags
    {
        VSync           = 0x1, ///< Presentation was vsync'd.
        HWClock         = 0x2, ///< Hardware provided the presentation timestamp.
        HWCompletion    = 0x4, ///< Hardware signaled the start of the presentation.
    };

    timespec time;          ///< The presentation timestamp.
    UInt32 period;          ///< Nanoseconds prediction until the next refresh. Zero if unknown.
    UInt64 frame;           ///< Vertical retrace counter. Zero if unknown.
    CZBitset<Flags> flags;  ///< Flags indicating how the presentation was done, see @ref SRM_PRESENTATION_TIME_FLAGS.
};

#endif // RPRESENTATIONTIME_H
