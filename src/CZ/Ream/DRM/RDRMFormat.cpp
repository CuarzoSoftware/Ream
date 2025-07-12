#include <CZ/Ream/DRM/RDRMFormat.h>
#include <drm_fourcc.h>
#include <unordered_map>

using namespace CZ;

class RFormatInfoMap : public std::unordered_map<RFormat, RFormatInfo>
{
public:
    RFormatInfoMap() noexcept
    {
        emplace(DRM_FORMAT_ARGB8888, RFormatInfo{ .format = DRM_FORMAT_ARGB8888, .bytesPerBlock = 4, .blockWidth = 1, .blockHeight = 1, .bpp = 32, .depth = 24, .planes = 1, .alpha = true});
        emplace(DRM_FORMAT_XRGB8888, RFormatInfo{ .format = DRM_FORMAT_XRGB8888, .bytesPerBlock = 4, .blockWidth = 1, .blockHeight = 1, .bpp = 32, .depth = 24, .planes = 1, .alpha = false});
        emplace(DRM_FORMAT_ABGR8888, RFormatInfo{ .format = DRM_FORMAT_ABGR8888, .bytesPerBlock = 4, .blockWidth = 1, .blockHeight = 1, .bpp = 32, .depth = 24, .planes = 1, .alpha = true});
        emplace(DRM_FORMAT_XBGR8888, RFormatInfo{ .format = DRM_FORMAT_XBGR8888, .bytesPerBlock = 4, .blockWidth = 1, .blockHeight = 1, .bpp = 32, .depth = 24, .planes = 1, .alpha = false});
    }
};

const RFormatInfo *RDRMFormat::GetInfo(RFormat format) noexcept
{
    static RFormatInfoMap map;
    auto it { map.find(format) };
    return it == map.end() ? nullptr : &it->second;
}


RFormat RDRMFormat::GetAlphaSubstitute(RFormat format) noexcept
{
    switch (format)
    {
    case DRM_FORMAT_XRGB4444:       return DRM_FORMAT_ARGB4444;
    case DRM_FORMAT_XBGR4444:       return DRM_FORMAT_ABGR4444;
    case DRM_FORMAT_RGBX4444:       return DRM_FORMAT_RGBA4444;
    case DRM_FORMAT_BGRX4444:       return DRM_FORMAT_BGRA4444;
    case DRM_FORMAT_XRGB1555:       return DRM_FORMAT_ARGB1555;
    case DRM_FORMAT_XBGR1555:       return DRM_FORMAT_ABGR1555;
    case DRM_FORMAT_RGBX5551:       return DRM_FORMAT_RGBA5551;
    case DRM_FORMAT_BGRX5551:       return DRM_FORMAT_BGRA5551;
    case DRM_FORMAT_XRGB8888:       return DRM_FORMAT_ARGB8888;
    case DRM_FORMAT_XBGR8888:       return DRM_FORMAT_ABGR8888;
    case DRM_FORMAT_RGBX8888:       return DRM_FORMAT_RGBA8888;
    case DRM_FORMAT_BGRX8888:       return DRM_FORMAT_BGRA8888;
    case DRM_FORMAT_XRGB2101010:    return DRM_FORMAT_ARGB2101010;
    case DRM_FORMAT_XBGR2101010:    return DRM_FORMAT_ABGR2101010;
    case DRM_FORMAT_RGBX1010102:    return DRM_FORMAT_RGBA1010102;
    case DRM_FORMAT_BGRX1010102:    return DRM_FORMAT_BGRA1010102;
    case DRM_FORMAT_XRGB16161616:   return DRM_FORMAT_ARGB16161616;
    case DRM_FORMAT_XBGR16161616:   return DRM_FORMAT_ABGR16161616;
    case DRM_FORMAT_XRGB16161616F:  return DRM_FORMAT_ARGB16161616F;
    case DRM_FORMAT_XBGR16161616F:  return DRM_FORMAT_ABGR16161616F;

    case DRM_FORMAT_ARGB4444:       return DRM_FORMAT_XRGB4444;
    case DRM_FORMAT_ABGR4444:       return DRM_FORMAT_XBGR4444;
    case DRM_FORMAT_RGBA4444:       return DRM_FORMAT_RGBX4444;
    case DRM_FORMAT_BGRA4444:       return DRM_FORMAT_BGRX4444;
    case DRM_FORMAT_ARGB1555:       return DRM_FORMAT_XRGB1555;
    case DRM_FORMAT_ABGR1555:       return DRM_FORMAT_XBGR1555;
    case DRM_FORMAT_RGBA5551:       return DRM_FORMAT_RGBX5551;
    case DRM_FORMAT_BGRA5551:       return DRM_FORMAT_BGRX5551;
    case DRM_FORMAT_ARGB8888:       return DRM_FORMAT_XRGB8888;
    case DRM_FORMAT_ABGR8888:       return DRM_FORMAT_XBGR8888;
    case DRM_FORMAT_RGBA8888:       return DRM_FORMAT_RGBX8888;
    case DRM_FORMAT_BGRA8888:       return DRM_FORMAT_BGRX8888;
    case DRM_FORMAT_ARGB2101010:    return DRM_FORMAT_XRGB2101010;
    case DRM_FORMAT_ABGR2101010:    return DRM_FORMAT_XBGR2101010;
    case DRM_FORMAT_RGBA1010102:    return DRM_FORMAT_RGBX1010102;
    case DRM_FORMAT_BGRA1010102:    return DRM_FORMAT_BGRX1010102;
    case DRM_FORMAT_ARGB16161616:   return DRM_FORMAT_XRGB16161616;
    case DRM_FORMAT_ABGR16161616:   return DRM_FORMAT_XBGR16161616;
    case DRM_FORMAT_ARGB16161616F:  return DRM_FORMAT_XRGB16161616F;
    case DRM_FORMAT_ABGR16161616F:  return DRM_FORMAT_XBGR16161616F;
    default:                        return format;
    }
}

RDRMFormatSet RDRMFormatSet::Union(const RDRMFormatSet &a, const RDRMFormatSet &b) noexcept
{
    RDRMFormatSet newSet {};

    for (const auto &format : a.formats())
        for (auto modifier : format.modifiers())
            newSet.add(format.format(), modifier);

    for (const auto &format : b.formats())
        for (auto modifier : format.modifiers())
            newSet.add(format.format(), modifier);

    return newSet;
}

RDRMFormatSet RDRMFormatSet::Intersect(const RDRMFormatSet &a, const RDRMFormatSet &b) noexcept
{
    RDRMFormatSet newSet {};

    for (const auto &format : a.formats())
        for (auto modifier : format.modifiers())
            if (b.has(format.format(), modifier))
                newSet.add(format.format(), modifier);

    return newSet;
}
