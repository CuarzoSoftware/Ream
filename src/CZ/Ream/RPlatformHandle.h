#ifndef RPLATFORMHANDLE_H
#define RPLATFORMHANDLE_H

#include <CZ/Ream/RObject.h>
#include <CZ/Ream/Ream.h>

/**
 * @brief Abstract handle to the underlying windowing/display platform.
 *
 * Base class for platform-specific handles (Wayland, DRM, offscreen). It exposes the
 * platform kind and safe down-casts to the concrete handle types.
 */
class CZ::RPlatformHandle : public RObject
{
public:
    /**
     * @brief Returns the platform this handle belongs to.
     *
     * @return The platform kind.
     */
    RPlatform platform() const noexcept { return m_platform; }

    /**
     * @brief Casts this handle to a Wayland platform handle.
     *
     * @return A pointer to the RWLPlatformHandle, or `nullptr` if this is not a Wayland handle.
     */
    RWLPlatformHandle *asWL() noexcept
    {
        if (platform() == RPlatform::Wayland)
            return (RWLPlatformHandle*)this;

        return nullptr;
    }

    /**
     * @brief Casts this handle to a DRM platform handle.
     *
     * @return A pointer to the RDRMPlatformHandle, or `nullptr` if this is not a DRM handle.
     */
    RDRMPlatformHandle *asDRM() noexcept
    {
        if (platform() == RPlatform::DRM)
            return (RDRMPlatformHandle*)this;

        return nullptr;
    }
protected:
    RPlatformHandle(RPlatform platform) noexcept :
        m_platform(platform) {}
    RPlatform m_platform;
};

#endif // RPLATFORMHANDLE_H
