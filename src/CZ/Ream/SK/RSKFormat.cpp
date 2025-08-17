#include <CZ/Ream/SK/RSKFormat.h>
#include <unordered_map>
#include <drm_fourcc.h>

using namespace CZ;

class RSKFromDRMMap : public std::unordered_map<RFormat, SkColorType>
{
public:
    RSKFromDRMMap() noexcept
    {
        emplace(DRM_FORMAT_ARGB8888,      kBGRA_8888_SkColorType);
        emplace(DRM_FORMAT_XRGB8888,      kBGRA_8888_SkColorType);
        emplace(DRM_FORMAT_ABGR8888,      kRGBA_8888_SkColorType);
        emplace(DRM_FORMAT_XBGR8888,      kRGB_888x_SkColorType);
        emplace(DRM_FORMAT_BGRA8888,      kBGRA_8888_SkColorType);
        emplace(DRM_FORMAT_RGB888,        kRGB_888x_SkColorType);
        emplace(DRM_FORMAT_BGR888,        kRGB_888x_SkColorType);

        emplace(DRM_FORMAT_RGB565,        kRGB_565_SkColorType);
        emplace(DRM_FORMAT_ARGB4444,      kARGB_4444_SkColorType);
        emplace(DRM_FORMAT_RGBA4444,      kARGB_4444_SkColorType);

        emplace(DRM_FORMAT_ABGR2101010,   kRGBA_1010102_SkColorType);
        emplace(DRM_FORMAT_XBGR2101010,   kRGB_101010x_SkColorType);
        emplace(DRM_FORMAT_ARGB2101010,   kBGRA_1010102_SkColorType);
        emplace(DRM_FORMAT_XRGB2101010,   kBGR_101010x_SkColorType);

        emplace(DRM_FORMAT_ABGR16161616F, kRGBA_F16_SkColorType);
        emplace(DRM_FORMAT_XBGR16161616F, kRGB_F16F16F16x_SkColorType);
        emplace(DRM_FORMAT_ABGR16161616,  kR16G16B16A16_unorm_SkColorType);
        emplace(DRM_FORMAT_GR88,          kR8G8_unorm_SkColorType);
        emplace(DRM_FORMAT_R8,            kR8_unorm_SkColorType);
        emplace(DRM_FORMAT_R16,           kA16_unorm_SkColorType);
        emplace(DRM_FORMAT_GR1616,        kR16G16_unorm_SkColorType);
    }
};

SkColorType RSKFormat::FromDRM(RFormat format) noexcept
{
    static RSKFromDRMMap map;
    auto it { map.find(format) };
    return it == map.end() ? kUnknown_SkColorType : it->second;
}

class RDRMFromSKMap : public std::unordered_map<SkColorType, RFormat>
{
public:
    RDRMFromSKMap() noexcept
    {
        emplace(kAlpha_8_SkColorType,             DRM_FORMAT_R8);
        emplace(kRGB_565_SkColorType,             DRM_FORMAT_RGB565);
        emplace(kARGB_4444_SkColorType,           DRM_FORMAT_ARGB4444);
        emplace(kRGBA_8888_SkColorType,           DRM_FORMAT_ABGR8888);
        emplace(kRGB_888x_SkColorType,            DRM_FORMAT_XBGR8888);
        emplace(kBGRA_8888_SkColorType,           DRM_FORMAT_ARGB8888);
        emplace(kRGBA_1010102_SkColorType,        DRM_FORMAT_ABGR2101010);
        emplace(kBGRA_1010102_SkColorType,        DRM_FORMAT_ARGB2101010);
        emplace(kRGB_101010x_SkColorType,         DRM_FORMAT_XBGR2101010);
        emplace(kBGR_101010x_SkColorType,         DRM_FORMAT_XRGB2101010);
        emplace(kGray_8_SkColorType,              DRM_FORMAT_R8);
        emplace(kRGBA_F16Norm_SkColorType,        DRM_FORMAT_ABGR16161616F);
        emplace(kRGBA_F16_SkColorType,            DRM_FORMAT_ABGR16161616F);
        emplace(kRGB_F16F16F16x_SkColorType,      DRM_FORMAT_XBGR16161616F);
        emplace(kR8G8_unorm_SkColorType,          DRM_FORMAT_GR88);
        emplace(kA16_float_SkColorType,           DRM_FORMAT_INVALID);
        emplace(kR16G16_unorm_SkColorType,        DRM_FORMAT_GR1616);
        emplace(kR16G16B16A16_unorm_SkColorType,  DRM_FORMAT_ABGR16161616);
        emplace(kSRGBA_8888_SkColorType,          DRM_FORMAT_ABGR8888);
        emplace(kR8_unorm_SkColorType,            DRM_FORMAT_R8);
    }
};

RFormat RSKFormat::ToDRM(SkColorType format) noexcept
{
    static RDRMFromSKMap map;
    auto it { map.find(format) };
    return it == map.end() ? DRM_FORMAT_INVALID : it->second;
}


const std::unordered_set<RFormat> &RSKFormat::SupportedFormats() noexcept
{
    static const std::unordered_set<RFormat> formats
    {
        DRM_FORMAT_ARGB8888,
        DRM_FORMAT_XRGB8888,
        DRM_FORMAT_ABGR8888,
        DRM_FORMAT_XBGR8888,
        DRM_FORMAT_BGRA8888,
        DRM_FORMAT_RGB888,
        DRM_FORMAT_BGR888,

        DRM_FORMAT_RGB565,
        DRM_FORMAT_ARGB4444,
        DRM_FORMAT_RGBA4444,

        DRM_FORMAT_ABGR2101010,
        DRM_FORMAT_XBGR2101010,
        DRM_FORMAT_ARGB2101010,
        DRM_FORMAT_XRGB2101010,

        DRM_FORMAT_ABGR16161616F,
        DRM_FORMAT_XBGR16161616F,
        DRM_FORMAT_ABGR16161616,
        DRM_FORMAT_GR88,
        DRM_FORMAT_R8,
        DRM_FORMAT_R16,
        DRM_FORMAT_GR1616
    };

    return formats;
}
