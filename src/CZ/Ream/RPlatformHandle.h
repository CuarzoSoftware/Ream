#ifndef RPLATFORMHANDLE_H
#define RPLATFORMHANDLE_H

#include <CZ/Ream/RObject.h>
#include <CZ/Ream/Ream.h>

class CZ::RPlatformHandle : public RObject
{
public:
    RPlatform platform() const noexcept { return m_platform; }

    RWLPlatformHandle *asWL() noexcept
    {
        if (platform() == RPlatform::Wayland)
            return (RWLPlatformHandle*)this;

        return nullptr;
    }

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
