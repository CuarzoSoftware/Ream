#include <CZ/Ream/GL/RGLImage.h>
#include <CZ/Ream/GL/RGLDevice.h>
#include <CZ/Ream/GL/RGLCore.h>
#include <CZ/Ream/GL/RGLMakeCurrent.h>
#include <CZ/Ream/EGL/REGLImage.h>
#include <CZ/Ream/GBM/RGBMBo.h>
#include <CZ/Ream/DRM/RDRMFramebuffer.h>
#include <CZ/Ream/RSync.h>
#include <CZ/Ream/RLog.h>

#include <gbm.h>
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

std::optional<GLuint> RGLImage::glFb(RGLDevice *device) const noexcept
{
    if (!device)
        device = core().asGL()->mainDevice();

    auto *threadData { static_cast<ThreadDeviceData*>(m_threadDataManager->getData(device)) };

    if (threadData->hasFb)
        return threadData->fb;

    auto image { eglImage(device) };

    if (!image)
        return {};

    threadData->fb = image->fb();

    if (!threadData->fb)
        return {};

    threadData->hasFb = true;
    threadData->fbOwnership = CZOwnership::Borrow;
    return threadData->fb;
}

std::shared_ptr<REGLImage> RGLImage::eglImage(RGLDevice *device) const noexcept
{
    if (!device)
        device = core().asGL()->mainDevice();

    auto &data { m_devicesMap[device] };

    if (data.eglImage)
        return data.eglImage;

    auto bo { gbmBo(device) };

    if (!bo)
        return {};

    data.eglImage = REGLImage::MakeFromDMA(bo->dmaInfo(), device);
    return data.eglImage;
}

std::shared_ptr<RGLImage> RGLImage::Make(SkISize size, const RDRMFormat &format, RStorageType storageType, RGLDevice *allocator) noexcept
{
    auto core { RCore::Get() };

    if (!core)
    {
        RLog(CZError, CZLN, "Missing RCore");
        return {};
    }

    if (core->graphicsAPI() != RGraphicsAPI::GL)
    {
        RLog(CZError, CZLN, "Not an RGLCore");
        return {};
    }

    if (!allocator)
    {
        assert(core->mainDevice());
        allocator = core->mainDevice()->asGL();
    }

    if (size.isEmpty())
    {
        allocator->log(CZError, CZLN, "Invalid image dimensions ({}x{})", size.width(), size.height());
        return {};
    }

    const RFormatInfo *formatInfo { RDRMFormat::GetInfo(format.format()) };

    if (!formatInfo)
    {
        allocator->log(CZError, CZLN, "Unsupported image format {}", drmGetFormatName(format.format()));
        return {};
    }

    const SkAlphaType alphaType { formatInfo->alpha ? kPremul_SkAlphaType : kOpaque_SkAlphaType };

    GlobalDeviceData data {};
    data.device = allocator;
    data.gbmBo = RGBMBo::Make(size, format, allocator);

    if (!data.gbmBo)
    {
        allocator->log(CZError, CZLN, "Failed to create gbm_bo");
        return {};
    }

    std::vector<RModifier> modifiers;
    modifiers.resize(data.gbmBo->planeCount());
    for (int i = 0; i < data.gbmBo->planeCount(); i++)
        modifiers[i] = data.gbmBo->modifier();

    auto image { std::shared_ptr<RGLImage>(new RGLImage(core, allocator, size, formatInfo, alphaType, modifiers)) };
    image->m_self = image;
    image->m_devicesMap.emplace(allocator, data);
    return image;
}

std::shared_ptr<RGLImage> RGLImage::MakeFromPixels(const RPixelBufferInfo &params, RGLDevice *allocator) noexcept
{
    auto core { RCore::Get() };

    if (!core)
    {
        RLog(CZError, CZLN, "Missing RCore");
        return {};
    }

    if (core->graphicsAPI() != RGraphicsAPI::GL)
    {
        RLog(CZError, CZLN, "Not an RGLCore");
        return {};
    }

    if (!allocator)
    {
        assert(core->mainDevice());
        allocator = core->mainDevice()->asGL();
    }

    if (params.size.isEmpty())
    {
        allocator->log(CZError, CZLN, "Invalid image dimensions ({}x{})", params.size.width(), params.size.height());
        return {};
    }

    const RFormatInfo *formatInfo { RDRMFormat::GetInfo(params.format) };

    if (!formatInfo)
    {
        allocator->log(CZError, CZLN, "Unsupported image format {}", drmGetFormatName(params.format));
        return {};
    }

    if (formatInfo->pixelsPerBlock() != 1)
    {
        allocator->log(CZError, CZLN, "Block formats are not supported: {}", drmGetFormatName(params.format));
        return {};
    }

    const RGLFormat *glFormat { RGLFormat::FromDRM(params.format) };

    if (!glFormat)
    {
        allocator->log(CZError, CZLN, "Failed to find GL equivalent for format: {}", drmGetFormatName(params.format));
        return {};
    }

    if (params.pixels && !formatInfo->validateStride(params.size.width(), params.stride))
    {
        allocator->log(CZError, CZLN, "Invalid stride {} for width {} and alignment {}", params.stride, params.size.width(), formatInfo->bytesPerBlock);
        return {};
    }

    SkAlphaType alphaType { formatInfo->alpha ? kPremul_SkAlphaType : kOpaque_SkAlphaType };

    if (formatInfo->alpha && params.alphaType == kUnpremul_SkAlphaType)
        alphaType = kUnpremul_SkAlphaType;

    auto image { std::shared_ptr<RGLImage>(new RGLImage(core, allocator, params.size, formatInfo, alphaType, { DRM_FORMAT_MOD_INVALID })) };
    image->m_self = image;

    auto current { RGLMakeCurrent::FromDevice(allocator, false) };
    GlobalDeviceData &data { image->m_devicesMap.emplace(allocator, GlobalDeviceData{}).first->second };
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

    image->setWriteSync(RSync::Make(allocator));
    return image;
}

std::shared_ptr<RGLImage> RGLImage::BorrowFramebuffer(const RGLFramebufferInfo &info, RGLDevice *allocator) noexcept
{
    auto core { RCore::Get() };

    if (!core)
    {
        RLog(CZError, CZLN, "Missing RCore");
        return {};
    }

    if (core->graphicsAPI() != RGraphicsAPI::GL)
    {
        RLog(CZError, CZLN, "Not an RGLCore");
        return {};
    }

    if (!allocator)
    {
        assert(core->mainDevice());
        allocator = core->mainDevice()->asGL();
    }

    if (info.size.isEmpty())
    {
        allocator->log(CZError, CZLN, "Invalid image dimensions ({}x{}).", info.size.width(), info.size.height());
        return {};
    }

    const RFormatInfo *formatInfo { RDRMFormat::GetInfo(info.format) };

    if (!formatInfo)
    {
        allocator->log(CZError, CZLN, "Unsupported image format {}", drmGetFormatName(info.format));
        return {};
    }

    // TODO: Validate

    SkAlphaType alphaType { formatInfo->alpha ? kPremul_SkAlphaType : kOpaque_SkAlphaType };

    if (formatInfo->alpha && info.alphaType == kUnpremul_SkAlphaType)
        alphaType = kUnpremul_SkAlphaType;

    auto image { std::shared_ptr<RGLImage>(new RGLImage(core, allocator, info.size, formatInfo, alphaType, {DRM_FORMAT_MOD_INVALID})) };
    image->m_self = image;
    auto *threadData { static_cast<ThreadDeviceData*>(image->m_threadDataManager->getData(allocator)) };
    threadData->fb = info.id;
    threadData->hasFb = true;
    return image;
}

std::shared_ptr<RGBMBo> RGLImage::gbmBo(RDevice *device) const noexcept
{
    if (!device)
        device = core().mainDevice();

    auto *dev { static_cast<RGLDevice*>(device) };
    auto &data { m_devicesMap[dev] };
    return data.gbmBo;
}

std::shared_ptr<RDRMFramebuffer> RGLImage::drmFb(RDevice *device) const noexcept
{
    if (!device)
        device = core().mainDevice();

    auto *dev { static_cast<RGLDevice*>(device) };
    auto &data { m_devicesMap[dev] };

    if (data.drmFb)
        return data.drmFb;

    data.drmFb = RDRMFramebuffer::MakeFromGBMBo(gbmBo(device));
    return data.drmFb;
}

bool RGLImage::writePixels(const RPixelBufferRegion &region) noexcept
{
    if (!region.pixels)
        return false;

    // TODO: Check bounds
    auto current { RGLMakeCurrent::FromDevice(allocator(), false) };

    return false;
}

RGLImage::GlobalDeviceDataMap::~GlobalDeviceDataMap() noexcept
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

RGLImage::ThreadDeviceData::~ThreadDeviceData() noexcept
{
    if (fbOwnership == CZOwnership::Own && fb)
        glDeleteFramebuffers(1, &fb);
}
