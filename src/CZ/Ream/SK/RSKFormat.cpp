#include <CZ/Ream/SK/RSKFormat.h>
#include <unordered_map>
#include <drm_fourcc.h>

using namespace CZ;

/* Unsized formats */
#define GR_GL_STENCIL_INDEX                  0x1901
#define GR_GL_DEPTH_COMPONENT                0x1902
#define GR_GL_DEPTH_STENCIL                  0x84F9
#define GR_GL_RED                            0x1903
#define GR_GL_RED_INTEGER                    0x8D94
#define GR_GL_GREEN                          0x1904
#define GR_GL_BLUE                           0x1905
#define GR_GL_ALPHA                          0x1906
#define GR_GL_LUMINANCE                      0x1909
#define GR_GL_LUMINANCE_ALPHA                0x190A
#define GR_GL_RG_INTEGER                     0x8228
#define GR_GL_RGB                            0x1907
#define GR_GL_RGB_INTEGER                    0x8D98
#define GR_GL_SRGB                           0x8C40
#define GR_GL_RGBA                           0x1908
#define GR_GL_RG                             0x8227
#define GR_GL_SRGB_ALPHA                     0x8C42
#define GR_GL_RGBA_INTEGER                   0x8D99
#define GR_GL_BGRA                           0x80E1

/* Stencil index sized formats */
#define GR_GL_STENCIL_INDEX4                 0x8D47
#define GR_GL_STENCIL_INDEX8                 0x8D48
#define GR_GL_STENCIL_INDEX16                0x8D49

/* Depth component sized formats */
#define GR_GL_DEPTH_COMPONENT16              0x81A5

/* Depth stencil sized formats */
#define GR_GL_DEPTH24_STENCIL8               0x88F0

/* Red sized formats */
#define GR_GL_R8                             0x8229
#define GR_GL_R16                            0x822A
#define GR_GL_R16F                           0x822D
#define GR_GL_R32F                           0x822E

/* Red integer sized formats */
#define GR_GL_R8I                            0x8231
#define GR_GL_R8UI                           0x8232
#define GR_GL_R16I                           0x8233
#define GR_GL_R16UI                          0x8234
#define GR_GL_R32I                           0x8235
#define GR_GL_R32UI                          0x8236

/* Luminance sized formats */
#define GR_GL_LUMINANCE8                     0x8040
#define GR_GL_LUMINANCE8_ALPHA8              0x8045
#define GR_GL_LUMINANCE16F                   0x881E

/* Alpha sized formats */
#define GR_GL_ALPHA8                         0x803C
#define GR_GL_ALPHA16                        0x803E
#define GR_GL_ALPHA16F                       0x881C
#define GR_GL_ALPHA32F                       0x8816

/* Alpha integer sized formats */
#define GR_GL_ALPHA8I                        0x8D90
#define GR_GL_ALPHA8UI                       0x8D7E
#define GR_GL_ALPHA16I                       0x8D8A
#define GR_GL_ALPHA16UI                      0x8D78
#define GR_GL_ALPHA32I                       0x8D84
#define GR_GL_ALPHA32UI                      0x8D72

/* RG sized formats */
#define GR_GL_RG8                            0x822B
#define GR_GL_RG16                           0x822C
#define GR_GL_R16F                           0x822D
#define GR_GL_R32F                           0x822E
#define GR_GL_RG16F                          0x822F

/* RG sized integer formats */
#define GR_GL_RG8I                           0x8237
#define GR_GL_RG8UI                          0x8238
#define GR_GL_RG16I                          0x8239
#define GR_GL_RG16UI                         0x823A
#define GR_GL_RG32I                          0x823B
#define GR_GL_RG32UI                         0x823C

/* RGB sized formats */
#define GR_GL_RGB5                           0x8050
#define GR_GL_RGB565                         0x8D62
#define GR_GL_RGB8                           0x8051
#define GR_GL_SRGB8                          0x8C41
#define GR_GL_RGBX8                          0x96BA

/* RGB integer sized formats */
#define GR_GL_RGB8I                          0x8D8F
#define GR_GL_RGB8UI                         0x8D7D
#define GR_GL_RGB16I                         0x8D89
#define GR_GL_RGB16UI                        0x8D77
#define GR_GL_RGB32I                         0x8D83
#define GR_GL_RGB32UI                        0x8D71

/* RGBA sized formats */
#define GR_GL_RGBA4                          0x8056
#define GR_GL_RGB5_A1                        0x8057
#define GR_GL_RGBA8                          0x8058
#define GR_GL_RGB10_A2                       0x8059
#define GR_GL_SRGB8_ALPHA8                   0x8C43
#define GR_GL_RGBA16F                        0x881A
#define GR_GL_RGBA32F                        0x8814
#define GR_GL_RG32F                          0x8230
#define GR_GL_RGBA16                         0x805B

/* RGBA integer sized formats */
#define GR_GL_RGBA8I                         0x8D8E
#define GR_GL_RGBA8UI                        0x8D7C
#define GR_GL_RGBA16I                        0x8D88
#define GR_GL_RGBA16UI                       0x8D76
#define GR_GL_RGBA32I                        0x8D82
#define GR_GL_RGBA32UI                       0x8D70

/* BGRA sized formats */
#define GR_GL_BGRA8                          0x93A1


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

