#include <CZ/Ream/DRM/RDRMFormat.h>
#include <CZ/Ream/RLockGuard.h>
#include <cstdio>
#include <drm_fourcc.h>
#include <unordered_map>
#include <xf86drm.h>

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

        emplace(DRM_FORMAT_A8, RFormatInfo{ .format = DRM_FORMAT_A8, .bytesPerBlock = 1, .blockWidth = 1, .blockHeight = 1, .bpp = 8, .depth = 0, .planes = 1, .alpha = true});
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

/* DRM get-name function results require manual freeing, but storing them here makes the leak harmless. */
static std::unordered_map<RFormat, std::string_view> FormatNames;
static std::unordered_map<RModifier, std::string_view> ModifierNames;

std::string_view RDRMFormat::FormatName(RFormat format) noexcept
{
    RLockGuard lock {};
    auto &name { FormatNames[format] };

    if (name.empty())
    {
        char *drmName { drmGetFormatName(format) };
        name = drmName ? drmName : "Unknown";
    }

    return name;
}

std::string_view RDRMFormat::ModifierName(RModifier modifier) noexcept
{
    RLockGuard lock {};
    auto &name { ModifierNames[modifier] };

    if (name.empty())
    {
        char *drmName { drmGetFormatModifierName(modifier) };
        name = drmName ? drmName : "Unknown";
    }

    return name;
}

const std::unordered_set<RFormat> &RDRMFormat::SupportedFormats() noexcept
{
    static const std::unordered_set<RFormat> formats {
        DRM_FORMAT_ARGB8888,
        DRM_FORMAT_XRGB8888,
        DRM_FORMAT_ABGR8888,
        DRM_FORMAT_XBGR8888,

        DRM_FORMAT_A8,
        DRM_FORMAT_C8,
        DRM_FORMAT_RGB332,
        DRM_FORMAT_BGR233,
        DRM_FORMAT_RGB565,
        DRM_FORMAT_BGR565,
        DRM_FORMAT_ARGB4444,
        DRM_FORMAT_ABGR4444,
        DRM_FORMAT_XRGB4444,
        DRM_FORMAT_XBGR4444,
        DRM_FORMAT_ARGB1555,
        DRM_FORMAT_ABGR1555,
        DRM_FORMAT_XRGB1555,
        DRM_FORMAT_XBGR1555,
        DRM_FORMAT_ARGB2101010,
        DRM_FORMAT_ABGR2101010,
        DRM_FORMAT_XRGB2101010,
        DRM_FORMAT_XBGR2101010,
        DRM_FORMAT_RGBA1010102,
        DRM_FORMAT_BGRA1010102,

        DRM_FORMAT_XRGB16161616F,
        DRM_FORMAT_XBGR16161616F,
        DRM_FORMAT_ARGB16161616F,
        DRM_FORMAT_ABGR16161616F,

        DRM_FORMAT_NV24,
        DRM_FORMAT_NV42,
        DRM_FORMAT_NV12,
        DRM_FORMAT_NV21,
        DRM_FORMAT_NV16,
        DRM_FORMAT_NV61,
        DRM_FORMAT_YUYV,
        DRM_FORMAT_UYVY,
        DRM_FORMAT_YVYU,
        DRM_FORMAT_VYUY,
        DRM_FORMAT_YUV420,
        DRM_FORMAT_YVU420,
        DRM_FORMAT_YUV422,
        DRM_FORMAT_YVU422,
        DRM_FORMAT_YUV444,
        DRM_FORMAT_YVU444,
        DRM_FORMAT_P010,
        DRM_FORMAT_P012,
        DRM_FORMAT_P016,
        DRM_FORMAT_Y210,
        DRM_FORMAT_Y212,
        DRM_FORMAT_Y216,
        DRM_FORMAT_XYUV8888,
        DRM_FORMAT_XVYU2101010,
        DRM_FORMAT_XVYU12_16161616,
        DRM_FORMAT_XVYU16161616
    };

    return formats;
}

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

RDRMFormatSet RDRMFormatSet::Intersect(const RDRMFormatSet &a, const REAM_FLAT_SET<RFormat> &b) noexcept
{
    RDRMFormatSet newSet {};

    for (const auto &format : a.formats())
        if (b.contains(format.format()))
            newSet.m_formats.emplace(format);

    return newSet;
}

bool RDRMFormatSet::has(RFormat format, RModifier modifier) const noexcept
{
    auto it { m_formats.find(format) };
    if (it == m_formats.end())
        return false;

    return it->modifiers().contains(modifier);
}

bool RDRMFormatSet::add(RFormat format, RModifier modifier) noexcept
{
    auto it { m_formats.find(format) };

    if (it == m_formats.end())
    {
        RDRMFormat newFormat { format, { modifier } };
        m_formats.insert(newFormat);
        return true;
    }
    else
        return it->addModifier(modifier);
}

bool RDRMFormatSet::remove(RFormat format, RModifier modifier) noexcept
{
    auto it { m_formats.find(format) };

    if (it == m_formats.end())
        return false;
    else
    {
        const bool ret { it->removeModifier(modifier) };
        if (it->modifiers().empty())
            m_formats.erase(it);
        return ret;
    }
}

bool RDRMFormatSet::removeFormat(RFormat format) noexcept
{
    auto it { m_formats.find(format) };

    if (it == m_formats.end())
        return false;
    else
    {
        m_formats.erase(it);
        return true;
    }
}

bool RDRMFormatSet::removeModifier(RModifier modifier) noexcept
{
    bool removedOne { false };

start:
    for (auto &format : m_formats)
    {
        removedOne |= format.removeModifier(modifier);

        if (format.modifiers().empty())
        {
            m_formats.erase(format);
            goto start;
        }
    }

    return removedOne;
}

bool RDRMFormatSet::addModifier(RModifier modifier) noexcept
{
    bool addedOne { false };

    for (auto &format : m_formats)
        addedOne |= format.addModifier(modifier);

    return addedOne;
}

void RDRMFormatSet::log() const noexcept
{
    if (formats().empty())
    {
        printf("Empty\n");
        return;
    }

    for (const auto &format : formats())
    {
        printf("%s: [", RDRMFormat::FormatName(format.format()).data());

        for (auto it = format.modifiers().begin(); it != format.modifiers().end(); it++)
        {
            if (it == std::prev(format.modifiers().end()))
                printf("%s", RDRMFormat::ModifierName(*it).data());
            else
                printf("%s, ", RDRMFormat::ModifierName(*it).data());
        }

        printf("]\n");
    }
}
