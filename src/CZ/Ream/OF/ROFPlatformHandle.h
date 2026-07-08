#ifndef ROFPLATFORMHANDLE_H
#define ROFPLATFORMHANDLE_H

#include <CZ/Ream/RPlatformHandle.h>
#include <memory>

/**
 * @brief Platform handle for the Offscreen platform.
 *
 * An RPlatformHandle implementation (see RPlatformHandle) whose platform() is
 * RPlatform::Offscreen. Unlike the DRM or Wayland handles, it carries no external resources and
 * simply identifies the offscreen platform (i.e. rendering without a display backend).
 *
 * Use Make() to create an instance.
 */
class CZ::ROFPlatformHandle : public RPlatformHandle
{
public:
    /**
     * @brief Creates an offscreen platform handle.
     *
     * @return A shared pointer to the created handle. Never fails.
     */
    static std::shared_ptr<ROFPlatformHandle> Make() noexcept { return std::shared_ptr<ROFPlatformHandle>(new ROFPlatformHandle()); };
private:
    ROFPlatformHandle() noexcept : RPlatformHandle(RPlatform::Offscreen) {}
};

#endif // ROFPLATFORMHANDLE_H
