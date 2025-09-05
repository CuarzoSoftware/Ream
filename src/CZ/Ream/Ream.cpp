#include <CZ/Ream/Ream.h>

using namespace CZ;

std::string_view CZ::RGraphicsAPIString(RGraphicsAPI api) noexcept
{
    switch (api)
    {
    case RGraphicsAPI::VK:
        return "Vulkan";
    case RGraphicsAPI::GL:
        return "OpenGL ES 2.0";
    case RGraphicsAPI::RS:
        return "Raster";
    default:
        return "Unknown";
    }
}

std::string_view CZ::RPlatformString(RPlatform platform) noexcept
{
    switch (platform)
    {
    case RPlatform::DRM:
        return "DRM";
    case RPlatform::Wayland:
        return "Wayland";
    case RPlatform::Offscreen:
        return "Offscreen";
    default:
        return "Unknown";
    }
}
