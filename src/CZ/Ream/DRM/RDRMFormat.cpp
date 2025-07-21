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

        emplace(DRM_FORMAT_C8, RFormatInfo{ .format = DRM_FORMAT_C8, .bytesPerBlock = 1, .blockWidth = 1, .blockHeight = 1, .bpp = 8, .depth = 8, .planes = 1, .alpha = false});
        emplace(DRM_FORMAT_RGB332,   RFormatInfo{ .format = DRM_FORMAT_RGB332,   .bytesPerBlock = 1, .blockWidth = 1, .blockHeight = 1, .bpp = 8,   .depth = 8,  .planes = 1, .alpha = false });
        emplace(DRM_FORMAT_BGR233,   RFormatInfo{ .format = DRM_FORMAT_BGR233,   .bytesPerBlock = 1, .blockWidth = 1, .blockHeight = 1, .bpp = 8,   .depth = 8,  .planes = 1, .alpha = false });
        emplace(DRM_FORMAT_RGB565,   RFormatInfo{ .format = DRM_FORMAT_RGB565,   .bytesPerBlock = 2, .blockWidth = 1, .blockHeight = 1, .bpp = 16,  .depth = 16, .planes = 1, .alpha = false });
        emplace(DRM_FORMAT_BGR565,   RFormatInfo{ .format = DRM_FORMAT_BGR565,   .bytesPerBlock = 2, .blockWidth = 1, .blockHeight = 1, .bpp = 16,  .depth = 16, .planes = 1, .alpha = false });
        emplace(DRM_FORMAT_ARGB4444, RFormatInfo{ .format = DRM_FORMAT_ARGB4444, .bytesPerBlock = 2, .blockWidth = 1, .blockHeight = 1, .bpp = 16,  .depth = 12, .planes = 1, .alpha = true });
        emplace(DRM_FORMAT_ABGR4444, RFormatInfo{ .format = DRM_FORMAT_ABGR4444, .bytesPerBlock = 2, .blockWidth = 1, .blockHeight = 1, .bpp = 16,  .depth = 12, .planes = 1, .alpha = true });
        emplace(DRM_FORMAT_XRGB4444, RFormatInfo{ .format = DRM_FORMAT_XRGB4444, .bytesPerBlock = 2, .blockWidth = 1, .blockHeight = 1, .bpp = 16,  .depth = 12, .planes = 1, .alpha = false });
        emplace(DRM_FORMAT_XBGR4444, RFormatInfo{ .format = DRM_FORMAT_XBGR4444, .bytesPerBlock = 2, .blockWidth = 1, .blockHeight = 1, .bpp = 16,  .depth = 12, .planes = 1, .alpha = false });
        emplace(DRM_FORMAT_ARGB1555, RFormatInfo{ .format = DRM_FORMAT_ARGB1555, .bytesPerBlock = 2, .blockWidth = 1, .blockHeight = 1, .bpp = 16,  .depth = 15, .planes = 1, .alpha = true });
        emplace(DRM_FORMAT_ABGR1555, RFormatInfo{ .format = DRM_FORMAT_ABGR1555, .bytesPerBlock = 2, .blockWidth = 1, .blockHeight = 1, .bpp = 16,  .depth = 15, .planes = 1, .alpha = true });
        emplace(DRM_FORMAT_XRGB1555, RFormatInfo{ .format = DRM_FORMAT_XRGB1555, .bytesPerBlock = 2, .blockWidth = 1, .blockHeight = 1, .bpp = 16,  .depth = 15, .planes = 1, .alpha = false });
        emplace(DRM_FORMAT_XBGR1555, RFormatInfo{ .format = DRM_FORMAT_XBGR1555, .bytesPerBlock = 2, .blockWidth = 1, .blockHeight = 1, .bpp = 16,  .depth = 15, .planes = 1, .alpha = false });
        emplace(DRM_FORMAT_ARGB2101010, RFormatInfo{ .format = DRM_FORMAT_ARGB2101010, .bytesPerBlock = 4, .blockWidth = 1, .blockHeight = 1, .bpp = 32, .depth = 30, .planes = 1, .alpha = true });
        emplace(DRM_FORMAT_ABGR2101010, RFormatInfo{ .format = DRM_FORMAT_ABGR2101010, .bytesPerBlock = 4, .blockWidth = 1, .blockHeight = 1, .bpp = 32, .depth = 30, .planes = 1, .alpha = true });
        emplace(DRM_FORMAT_XRGB2101010, RFormatInfo{ .format = DRM_FORMAT_XRGB2101010, .bytesPerBlock = 4, .blockWidth = 1, .blockHeight = 1, .bpp = 32, .depth = 30, .planes = 1, .alpha = false });
        emplace(DRM_FORMAT_XBGR2101010, RFormatInfo{ .format = DRM_FORMAT_XBGR2101010, .bytesPerBlock = 4, .blockWidth = 1, .blockHeight = 1, .bpp = 32, .depth = 30, .planes = 1, .alpha = false });
        emplace(DRM_FORMAT_RGBA1010102, RFormatInfo{ .format = DRM_FORMAT_RGBA1010102, .bytesPerBlock = 4, .blockWidth = 1, .blockHeight = 1, .bpp = 32, .depth = 30, .planes = 1, .alpha = true });
        emplace(DRM_FORMAT_BGRA1010102, RFormatInfo{ .format = DRM_FORMAT_BGRA1010102, .bytesPerBlock = 4, .blockWidth = 1, .blockHeight = 1, .bpp = 32, .depth = 30, .planes = 1, .alpha = true });

        emplace(DRM_FORMAT_XRGB16161616F, RFormatInfo{ .format = DRM_FORMAT_XRGB16161616F, .bytesPerBlock = 8, .blockWidth = 1, .blockHeight = 1, .bpp = 64, .depth = 48, .planes = 1, .alpha = false});
        emplace(DRM_FORMAT_XBGR16161616F, RFormatInfo{ .format = DRM_FORMAT_XBGR16161616F, .bytesPerBlock = 8, .blockWidth = 1, .blockHeight = 1, .bpp = 64, .depth = 48, .planes = 1, .alpha = false});
        emplace(DRM_FORMAT_ARGB16161616F, RFormatInfo{ .format = DRM_FORMAT_ARGB16161616F, .bytesPerBlock = 8, .blockWidth = 1, .blockHeight = 1, .bpp = 64, .depth = 48, .planes = 1, .alpha = true});
        emplace(DRM_FORMAT_ABGR16161616F, RFormatInfo{ .format = DRM_FORMAT_ABGR16161616F, .bytesPerBlock = 8, .blockWidth = 1, .blockHeight = 1, .bpp = 64, .depth = 48, .planes = 1, .alpha = true});

        emplace(DRM_FORMAT_NV24, RFormatInfo{ .format = DRM_FORMAT_NV24, .bytesPerBlock = 1, .blockWidth = 1, .blockHeight = 1, .bpp = 24, .depth = 8, .planes = 2, .alpha = false});
        emplace(DRM_FORMAT_NV42, RFormatInfo{ .format = DRM_FORMAT_NV42, .bytesPerBlock = 1, .blockWidth = 1, .blockHeight = 1, .bpp = 24, .depth = 8, .planes = 2, .alpha = false});
        emplace(DRM_FORMAT_NV12, RFormatInfo{ .format = DRM_FORMAT_NV12, .bytesPerBlock = 1, .blockWidth = 1, .blockHeight = 1, .bpp = 12, .depth = 8, .planes = 2, .alpha = false});
        emplace(DRM_FORMAT_NV21, RFormatInfo{ .format = DRM_FORMAT_NV21, .bytesPerBlock = 1, .blockWidth = 1, .blockHeight = 1, .bpp = 12, .depth = 8, .planes = 2, .alpha = false});
        emplace(DRM_FORMAT_NV16, RFormatInfo{ .format = DRM_FORMAT_NV16, .bytesPerBlock = 1, .blockWidth = 1, .blockHeight = 1, .bpp = 16, .depth = 8, .planes = 2, .alpha = false});
        emplace(DRM_FORMAT_NV61, RFormatInfo{ .format = DRM_FORMAT_NV61, .bytesPerBlock = 1, .blockWidth = 1, .blockHeight = 1, .bpp = 16, .depth = 8, .planes = 2, .alpha = false});
        emplace(DRM_FORMAT_YUYV, RFormatInfo{ .format = DRM_FORMAT_YUYV, .bytesPerBlock = 2, .blockWidth = 2, .blockHeight = 1, .bpp = 16, .depth = 8, .planes = 1, .alpha = false});
        emplace(DRM_FORMAT_UYVY, RFormatInfo{ .format = DRM_FORMAT_UYVY, .bytesPerBlock = 2, .blockWidth = 2, .blockHeight = 1, .bpp = 16, .depth = 8, .planes = 1, .alpha = false});
        emplace(DRM_FORMAT_YVYU, RFormatInfo{ .format = DRM_FORMAT_YVYU, .bytesPerBlock = 2, .blockWidth = 2, .blockHeight = 1, .bpp = 16, .depth = 8, .planes = 1, .alpha = false});
        emplace(DRM_FORMAT_VYUY, RFormatInfo{ .format = DRM_FORMAT_VYUY, .bytesPerBlock = 2, .blockWidth = 2, .blockHeight = 1, .bpp = 16, .depth = 8, .planes = 1, .alpha = false});
        emplace(DRM_FORMAT_YUV420, RFormatInfo{ .format = DRM_FORMAT_YUV420, .bytesPerBlock = 1, .blockWidth = 1, .blockHeight = 1, .bpp = 12, .depth = 8, .planes = 3, .alpha = false});
        emplace(DRM_FORMAT_YVU420, RFormatInfo{ .format = DRM_FORMAT_YVU420, .bytesPerBlock = 1, .blockWidth = 1, .blockHeight = 1, .bpp = 12, .depth = 8, .planes = 3, .alpha = false});
        emplace(DRM_FORMAT_YUV422, RFormatInfo{ .format = DRM_FORMAT_YUV422, .bytesPerBlock = 1, .blockWidth = 1, .blockHeight = 1, .bpp = 16, .depth = 8, .planes = 3, .alpha = false});
        emplace(DRM_FORMAT_YVU422, RFormatInfo{ .format = DRM_FORMAT_YVU422, .bytesPerBlock = 1, .blockWidth = 1, .blockHeight = 1, .bpp = 16, .depth = 8, .planes = 3, .alpha = false});
        emplace(DRM_FORMAT_YUV444, RFormatInfo{ .format = DRM_FORMAT_YUV444, .bytesPerBlock = 1, .blockWidth = 1, .blockHeight = 1, .bpp = 24, .depth = 8, .planes = 3, .alpha = false});
        emplace(DRM_FORMAT_YVU444, RFormatInfo{ .format = DRM_FORMAT_YVU444, .bytesPerBlock = 1, .blockWidth = 1, .blockHeight = 1, .bpp = 24, .depth = 8, .planes = 3, .alpha = false});
        emplace(DRM_FORMAT_P010, RFormatInfo{ .format = DRM_FORMAT_P010, .bytesPerBlock = 2, .blockWidth = 1, .blockHeight = 1, .bpp = 24, .depth = 16, .planes = 2, .alpha = false});
        emplace(DRM_FORMAT_P012, RFormatInfo{ .format = DRM_FORMAT_P012, .bytesPerBlock = 2, .blockWidth = 1, .blockHeight = 1, .bpp = 24, .depth = 16, .planes = 2, .alpha = false});
        emplace(DRM_FORMAT_P016, RFormatInfo{ .format = DRM_FORMAT_P016, .bytesPerBlock = 2, .blockWidth = 1, .blockHeight = 1, .bpp = 24, .depth = 16, .planes = 2, .alpha = false});
        emplace(DRM_FORMAT_Y210, RFormatInfo{ .format = DRM_FORMAT_Y210, .bytesPerBlock = 4, .blockWidth = 2, .blockHeight = 1, .bpp = 32, .depth = 20, .planes = 1, .alpha = false});
        emplace(DRM_FORMAT_Y212, RFormatInfo{ .format = DRM_FORMAT_Y212, .bytesPerBlock = 4, .blockWidth = 2, .blockHeight = 1, .bpp = 32, .depth = 20, .planes = 1, .alpha = false});
        emplace(DRM_FORMAT_Y216, RFormatInfo{ .format = DRM_FORMAT_Y216, .bytesPerBlock = 4, .blockWidth = 2, .blockHeight = 1, .bpp = 32, .depth = 16, .planes = 1, .alpha = false});
        emplace(DRM_FORMAT_XYUV8888, RFormatInfo{ .format = DRM_FORMAT_XYUV8888, .bytesPerBlock = 4, .blockWidth = 1, .blockHeight = 1, .bpp = 32, .depth = 24, .planes = 1, .alpha = false});
        emplace(DRM_FORMAT_XVYU2101010, RFormatInfo{ .format = DRM_FORMAT_XVYU2101010, .bytesPerBlock = 4, .blockWidth = 1, .blockHeight = 1, .bpp = 32, .depth = 30, .planes = 1, .alpha = false});
        emplace(DRM_FORMAT_XVYU12_16161616, RFormatInfo{ .format = DRM_FORMAT_XVYU12_16161616, .bytesPerBlock = 8, .blockWidth = 1, .blockHeight = 1, .bpp = 64, .depth = 48, .planes = 1, .alpha = false});
        emplace(DRM_FORMAT_XVYU16161616, RFormatInfo{ .format = DRM_FORMAT_XVYU16161616, .bytesPerBlock = 8, .blockWidth = 1, .blockHeight = 1, .bpp = 64, .depth = 48, .planes = 1, .alpha = false});
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

RDRMFormatSet RDRMFormatSet::Intersect(const RDRMFormatSet &a, const std::flat_set<RFormat> &b) noexcept
{
    RDRMFormatSet newSet {};

    for (const auto &format : a.formats())
        if (b.contains(format.format()))
            newSet.m_formats.emplace(format);

    return newSet;
}
