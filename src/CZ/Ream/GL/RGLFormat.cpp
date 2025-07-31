#include <CZ/Ream/GL/RGLFormat.h>
#include <GLES2/gl2ext.h>
#include <drm_fourcc.h>
#include <unordered_map>

using namespace CZ;

class GLFromDRMMap : public std::unordered_map<RFormat, RGLFormat>
{
public:
    GLFromDRMMap() noexcept
    {
        emplace(DRM_FORMAT_ARGB8888,      RGLFormat{ .format = GL_BGRA_EXT, .internalFormat = GL_BGRA8_EXT,         .type = GL_UNSIGNED_BYTE });
        emplace(DRM_FORMAT_XRGB8888,      RGLFormat{ .format = GL_BGRA_EXT, .internalFormat = GL_BGRA8_EXT,         .type = GL_UNSIGNED_BYTE });
        emplace(DRM_FORMAT_XBGR8888,      RGLFormat{ .format = GL_RGBA,     .internalFormat = GL_RGBA8_OES,         .type = GL_UNSIGNED_BYTE });
        emplace(DRM_FORMAT_ABGR8888,      RGLFormat{ .format = GL_RGBA,     .internalFormat = GL_RGBA8_OES,         .type = GL_UNSIGNED_BYTE });
        emplace(DRM_FORMAT_BGR888,        RGLFormat{ .format = GL_RGB,      .internalFormat = GL_RGB8_OES,          .type = GL_UNSIGNED_BYTE });

        // Wrong, but
        emplace(DRM_FORMAT_R8,            RGLFormat{ .format = GL_ALPHA,    .internalFormat = GL_ALPHA,             .type = GL_UNSIGNED_BYTE });

        if (std::endian::native == std::endian::little)
        {
            emplace(DRM_FORMAT_RGBX4444,      RGLFormat{ .format = GL_RGBA,     .internalFormat = GL_RGBA4,             .type = GL_UNSIGNED_SHORT_4_4_4_4 });
            emplace(DRM_FORMAT_RGBA4444,      RGLFormat{ .format = GL_RGBA,     .internalFormat = GL_RGBA4,             .type = GL_UNSIGNED_SHORT_4_4_4_4 });
            emplace(DRM_FORMAT_RGBX5551,      RGLFormat{ .format = GL_RGBA,     .internalFormat = GL_RGB5_A1,           .type = GL_UNSIGNED_SHORT_5_5_5_1 });
            emplace(DRM_FORMAT_RGBA5551,      RGLFormat{ .format = GL_RGBA,     .internalFormat = GL_RGB5_A1,           .type = GL_UNSIGNED_SHORT_5_5_5_1 });
            emplace(DRM_FORMAT_RGB565,        RGLFormat{ .format = GL_RGB,      .internalFormat = GL_RGB565,            .type = GL_UNSIGNED_SHORT_5_6_5   });
            emplace(DRM_FORMAT_XBGR2101010,   RGLFormat{ .format = GL_RGBA,     .internalFormat = GL_RGB10_A2_EXT,      .type = GL_UNSIGNED_INT_2_10_10_10_REV_EXT });
            emplace(DRM_FORMAT_ABGR2101010,   RGLFormat{ .format = GL_RGBA,     .internalFormat = GL_RGB10_A2_EXT,      .type = GL_UNSIGNED_INT_2_10_10_10_REV_EXT });
            emplace(DRM_FORMAT_XBGR16161616F, RGLFormat{ .format = GL_RGBA,     .internalFormat = GL_RGBA16F_EXT,       .type = GL_HALF_FLOAT_OES });
            emplace(DRM_FORMAT_ABGR16161616F, RGLFormat{ .format = GL_RGBA,     .internalFormat = GL_RGBA16F_EXT,       .type = GL_HALF_FLOAT_OES });
            emplace(DRM_FORMAT_XBGR16161616,  RGLFormat{ .format = GL_RGBA,     .internalFormat = GL_RGBA16_EXT,        .type = GL_UNSIGNED_SHORT });
            emplace(DRM_FORMAT_ABGR16161616,  RGLFormat{ .format = GL_RGBA,     .internalFormat = GL_RGBA16_EXT,        .type = GL_UNSIGNED_SHORT });
        }
    }
};

const RGLFormat *RGLFormat::FromDRM(RFormat format) noexcept
{
    static GLFromDRMMap map;
    auto it { map.find(format) };
    return it == map.end() ? nullptr : &it->second;
}
