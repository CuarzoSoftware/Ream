#include <CZ/Ream/GL/RGLImage.h>
#include <CZ/Ream/GL/RGLDevice.h>
#include <CZ/Ream/GL/RGLCore.h>
#include <CZ/Ream/GL/RGLMakeCurrent.h>

#include <CZ/Ream/RLog.h>

#include <xf86drm.h>
#include <drm_fourcc.h>

using namespace CZ;

RGLTexture RGLImage::texture(RGLDevice *device) const noexcept
{
    if (!device)
        device = core().asGL()->mainDevice();

    auto it { m_devicesMap.find(device) };
    if (it == m_devicesMap.end())
        return {};

    return it->second.texture;
}

std::optional<GLuint> RGLImage::framebuffer(RGLDevice *device) const noexcept
{
    if (!device)
        device = core().asGL()->mainDevice();

    auto it { m_devicesMap.find(device) };
    if (it == m_devicesMap.end() || !it->second.hasFb)
        return {};

    return it->second.fb;
}

std::shared_ptr<RGLImage> RGLImage::MakeFromPixels(const RPixelBufferInfo &params, RGLDevice *allocator) noexcept
{
    if (!RCore::Get())
    {
        RError(RLINE, "Cannot create an RImage without an RCore.");
        return {};
    }

    auto core { RCore::Get()->asGL() };

    if (!core)
    {
        RError(RLINE, "The current RGraphicsAPI is not GL.");
        return {};
    }

    if (!allocator)
    {
        assert(core->mainDevice());
        allocator = core->mainDevice()->asGL();
    }

    if (params.size.isEmpty())
    {
        RError(RLINE, "Invalid image dimensions (%dx%d).", params.size.width(), params.size.height());
        return {};
    }

    const RFormatInfo *formatInfo { RDRMFormat::GetInfo(params.format) };

    if (!formatInfo)
    {
        RError(RLINE, "Unsupported image format %s.", drmGetFormatName(params.format));
        return {};
    }

    if (formatInfo->pixelsPerBlock() != 1)
    {
        RError(RLINE, "Block formats are not supported: %s.", drmGetFormatName(params.format));
        return {};
    }

    const RGLFormat *glFormat { RGLFormat::FromDRM(params.format) };

    if (!glFormat)
    {
        RError(RLINE, "Failed to find GL equivalent for format %s.", drmGetFormatName(params.format));
        return {};
    }

    if (params.pixels && !formatInfo->validateStride(params.size.width(), params.stride))
    {
        RError(RLINE, "Invalid stride %d for width %d and alignment %d.", params.stride, params.size.width(), formatInfo->bytesPerBlock);
        return {};
    }

    auto image { std::shared_ptr<RGLImage>(new RGLImage(core, allocator, params.size, { .format = params.format, .modifier = DRM_FORMAT_MOD_INVALID })) };
    image->m_self = image;

    auto current { RGLMakeCurrent::FromDevice(allocator, false) };
    DeviceData &data { image->m_devicesMap.emplace(allocator, DeviceData{}).first->second };
    data.textureOwnership = CZOwnership::Own;
    data.device = allocator;
    data.texture.target = GL_TEXTURE_2D;
    glGenTextures(1, &data.texture.id);
    glBindTexture(data.texture.target, data.texture.id);

    if (params.pixels)
    {
        glPixelStorei(GL_UNPACK_ALIGNMENT, formatInfo->bytesPerBlock);
        glPixelStorei(GL_UNPACK_ROW_LENGTH_EXT, params.stride/formatInfo->bytesPerBlock);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS_EXT, 0);
        glPixelStorei(GL_UNPACK_SKIP_ROWS_EXT, 0);

        glTexImage2D(
            data.texture.target, 0, glFormat->internalFormat,
            params.size.width(), params.size.height(),
            0, glFormat->format, glFormat->type, params.pixels);

        glPixelStorei(GL_UNPACK_ALIGNMENT, 0);
        glPixelStorei(GL_UNPACK_ROW_LENGTH_EXT, 0);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS_EXT, 0);
        glPixelStorei(GL_UNPACK_SKIP_ROWS_EXT, 0);
    }
    else
    {
        glTexImage2D(
            data.texture.target, 0, glFormat->internalFormat,
            params.size.width(), params.size.height(),
            0, glFormat->format, glFormat->type, NULL);
    }

    return image;
}

std::shared_ptr<RGLImage> RGLImage::BorrowFramebuffer(const RGLFramebufferInfo &info, RGLDevice *allocator) noexcept
{
    if (!RCore::Get())
    {
        RError(RLINE, "Cannot create an RImage without an RCore.");
        return {};
    }

    auto core { RCore::Get()->asGL() };

    if (!core)
    {
        RError(RLINE, "The current RGraphicsAPI is not GL.");
        return {};
    }

    if (!allocator)
    {
        assert(core->mainDevice());
        allocator = core->mainDevice()->asGL();
    }

    if (info.size.isEmpty())
    {
        RError(RLINE, "Invalid image dimensions (%dx%d).", info.size.width(), info.size.height());
        return {};
    }

    // TODO: Validate

    auto image { std::shared_ptr<RGLImage>(new RGLImage(core, allocator, info.size, {info.format, DRM_FORMAT_MOD_INVALID})) };
    image->m_self = image;
    DeviceData data {};
    data.fb = info.id;
    data.hasFb = true;
    image->m_devicesMap.emplace(allocator, data);
    return image;
}

bool RGLImage::writePixels(const RPixelBufferRegion &region) noexcept
{
    if (!region.pixels)
        return false;

    // TODO: Check bounds
    auto current { RGLMakeCurrent::FromDevice(allocator(), false) };


    return false;
}

RGLImage::DeviceDataMap::~DeviceDataMap() noexcept
{
    while (!empty())
    {
        const auto &it { begin() };
        auto &data { it->second };

        if (data.textureOwnership == CZOwnership::Own && data.texture.id != 0)
        {
            auto current { RGLMakeCurrent::FromDevice(data.device, false) };
            glDeleteTextures(1, &data.texture.id);
        }

        erase(it);
    }
}
