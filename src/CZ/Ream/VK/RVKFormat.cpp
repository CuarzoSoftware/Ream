#include <CZ/Ream/VK/RVKFormat.h>
#include <drm_fourcc.h>
#include <unordered_map>

using namespace CZ;

// Little hack to support A8 (mirrors RDRMFormat.h)
#ifndef DRM_FORMAT_A8
#define DRM_FORMAT_A8 fourcc_code('A', '8', ' ', ' ')
#endif

// DRM FourCC codes describe the in-memory byte order (little-endian). The VkFormats below
// are chosen so their component byte layout matches the DRM code exactly.
static const std::unordered_map<RFormat, VkFormat> &VKFromDRMMap() noexcept
{
    static const std::unordered_map<RFormat, VkFormat> map
    {
        // 32-bit BGRA byte order (DRM *RGB8888)
        { DRM_FORMAT_ARGB8888,     VK_FORMAT_B8G8R8A8_UNORM },
        { DRM_FORMAT_XRGB8888,     VK_FORMAT_B8G8R8A8_UNORM },

        // 32-bit RGBA byte order (DRM *BGR8888)
        { DRM_FORMAT_ABGR8888,     VK_FORMAT_R8G8B8A8_UNORM },
        { DRM_FORMAT_XBGR8888,     VK_FORMAT_R8G8B8A8_UNORM },

        // 16-bit
        { DRM_FORMAT_RGB565,       VK_FORMAT_R5G6B5_UNORM_PACK16 },

        // 10-bit packed
        { DRM_FORMAT_ARGB2101010,  VK_FORMAT_A2R10G10B10_UNORM_PACK32 },
        { DRM_FORMAT_XRGB2101010,  VK_FORMAT_A2R10G10B10_UNORM_PACK32 },
        { DRM_FORMAT_ABGR2101010,  VK_FORMAT_A2B10G10R10_UNORM_PACK32 },
        { DRM_FORMAT_XBGR2101010,  VK_FORMAT_A2B10G10R10_UNORM_PACK32 },

        // 16-bit float
        { DRM_FORMAT_ABGR16161616F, VK_FORMAT_R16G16B16A16_SFLOAT },

        // single/dual channel
        { DRM_FORMAT_R8,           VK_FORMAT_R8_UNORM },
        { DRM_FORMAT_GR88,         VK_FORMAT_R8G8_UNORM },
    };
    return map;
}

VkFormat RVKFormat::FromDRM(RFormat format) noexcept
{
    const auto &map { VKFromDRMMap() };
    const auto it { map.find(format) };
    return it == map.end() ? VK_FORMAT_UNDEFINED : it->second;
}

RFormat RVKFormat::ToDRM(VkFormat format) noexcept
{
    // Prefer the alpha-bearing DRM code for each VkFormat.
    switch (format)
    {
    case VK_FORMAT_B8G8R8A8_UNORM:          return DRM_FORMAT_ARGB8888;
    case VK_FORMAT_R8G8B8A8_UNORM:          return DRM_FORMAT_ABGR8888;
    case VK_FORMAT_R5G6B5_UNORM_PACK16:     return DRM_FORMAT_RGB565;
    case VK_FORMAT_A2R10G10B10_UNORM_PACK32:return DRM_FORMAT_ARGB2101010;
    case VK_FORMAT_A2B10G10R10_UNORM_PACK32:return DRM_FORMAT_ABGR2101010;
    case VK_FORMAT_R16G16B16A16_SFLOAT:     return DRM_FORMAT_ABGR16161616F;
    case VK_FORMAT_R8_UNORM:                return DRM_FORMAT_R8;
    case VK_FORMAT_R8G8_UNORM:              return DRM_FORMAT_GR88;
    default:                                return DRM_FORMAT_INVALID;
    }
}
