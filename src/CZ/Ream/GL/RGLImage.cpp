#include <CZ/Ream/GL/RGLImage.h>
#include <CZ/Ream/GL/RGLDevice.h>
#include <CZ/Ream/GL/RGLCore.h>
#include <CZ/Ream/GL/RGLMakeCurrent.h>
#include <CZ/Ream/SK/RSKFormat.h>
#include <CZ/Ream/EGL/REGLImage.h>
#include <CZ/Ream/GBM/RGBMBo.h>
#include <CZ/Ream/DRM/RDRMFramebuffer.h>
#include <CZ/Ream/RSync.h>
#include <CZ/Ream/RLog.h>

#include <CZ/skia/gpu/ganesh/gl/GrGLTypes.h>
#include <CZ/skia/gpu/ganesh/GrBackendSurface.h>
#include <CZ/skia/gpu/ganesh/gl/GrGLBackendSurface.h>
#include <CZ/skia/gpu/ganesh/SkSurfaceGanesh.h>
#include <CZ/skia/gpu/ganesh/SkImageGanesh.h>
#include <CZ/skia/core/SkColorSpace.h>

#include <mutex>
#include <gbm.h>
#include <xf86drm.h>
#include <drm_fourcc.h>

static auto skSRGB { SkColorSpace::MakeSRGB() };
static std::recursive_mutex mutex;

using namespace CZ;

/* Validates common parameters during allocation */
static std::shared_ptr<RGLCore> ValidateMake(SkISize size, RFormat format, SkAlphaType alphaType, RGLDevice **allocator, const RFormatInfo **formatInfo, SkAlphaType *outAlphaType) noexcept
{
    auto core { RCore::Get() };

    if (!core)
    {
        RLog(CZError, CZLN, "Missing RCore");
        return {};
    }

    auto glCore { core->asGL() };

    if (!glCore)
    {
        RLog(CZError, CZLN, "Not an RGLCore");
        return {};
    }

    if (!*allocator)
    {
        assert(core->mainDevice());
        *allocator = core->mainDevice()->asGL();
    }

    if (size.isEmpty())
    {
        (*allocator)->log(CZError, CZLN, "Invalid image dimensions ({}x{})", size.width(), size.height());
        return {};
    }

    *formatInfo = RDRMFormat::GetInfo(format);

    if (!*formatInfo)
    {
        (*allocator)->log(CZError, CZLN, "Unsupported image format {}", RDRMFormat::FormatName(format));
        return {};
    }

    if (alphaType == kUnknown_SkAlphaType)
        *outAlphaType = (*formatInfo)->alpha ? kPremul_SkAlphaType : kOpaque_SkAlphaType;
    else if (alphaType == kOpaque_SkAlphaType && (*formatInfo)->alpha)
        *outAlphaType = kPremul_SkAlphaType;
    else
        *outAlphaType = alphaType;

    return glCore;
}

static bool ValidateConstraints(std::shared_ptr<RGLImage> image, const RImageConstraints *constraints) noexcept
{
    if (!constraints)
        return true;

    bool hasReadFormat { constraints->readFormats.empty() };

    for (auto fmt : constraints->readFormats)
    {
        if (image->readFormats().contains(fmt))
        {
            hasReadFormat = true;
            break;
        }
    }

    if (!hasReadFormat)
    {
        RLog(CZTrace, "None of the required RImage read formats are available");
        return false;
    }

    bool hasWriteFormat { constraints->writeFormats.empty() };

    for (auto fmt : constraints->writeFormats)
    {
        if (image->writeFormats().contains(fmt))
        {
            hasWriteFormat = true;
            break;
        }
    }

    if (!hasWriteFormat)
    {
        RLog(CZTrace, "None of the required RImage write formats are available");
        return false;
    }

    for (const auto &dev : constraints->caps)
    {
        assert(dev.first != nullptr && "nullptr was passed to RImageConstraints::caps");

        if (dev.second != image->checkDeviceCaps(dev.second, dev.first))
        {
            dev.first->log(CZTrace, "Required RImage caps not met");
            return false;
        }
    }

    return true;
}

RGLTexture RGLImage::texture(RGLDevice *device) const noexcept
{
    std::lock_guard<std::recursive_mutex> lock(mutex);
    if (!device)
        device = core()->asGL()->mainDevice();

    auto &deviceData { m_devicesMap[device] };

    /* Already has one */

    if (deviceData.texture.id != 0)
        return deviceData.texture;

    /* Already failed to create one */

    auto image { eglImage(device) };

    if (!image)
        return deviceData.texture;

    deviceData.texture = image->texture();
    deviceData.textureOwnership = CZOwn::Borrow;
    return deviceData.texture;
}

std::optional<GLuint> RGLImage::glFb(RGLDevice *device) const noexcept
{
    std::lock_guard<std::recursive_mutex> lock(mutex);
    if (!device)
        device = core()->asGL()->mainDevice();

    auto *contextData { static_cast<ContextData*>(m_contextDataManager->getData(device)) };

    /* Already has one */

    if (contextData->glFb.has_value())
        return contextData->glFb;

    auto &deviceData { m_devicesMap[device] };

    /* Already failed before */

    if (deviceData.unsupportedCaps.has(NoGLFramebufer))
        return {};

    /* Attempt to create one */

    auto image { eglImage(device) };

    if (!image)
    {
        deviceData.unsupportedCaps.add(NoGLFramebufer);
        return {};
    }

    contextData->glFb = image->fb();

    if (contextData->glFb.value() == 0)
    {
        deviceData.unsupportedCaps.add(NoGLFramebufer);
        contextData->glFb.reset();
        return contextData->glFb;
    }

    contextData->fbOwnership = CZOwn::Borrow;
    return contextData->glFb;
}

std::shared_ptr<REGLImage> RGLImage::eglImage(RGLDevice *device) const noexcept
{
    std::lock_guard<std::recursive_mutex> lock(mutex);

    if (!device)
        device = core()->asGL()->mainDevice();

    auto &deviceData { m_devicesMap[device] };

    /* Already has one */

    if (deviceData.eglImage)
        return deviceData.eglImage;

    /* Already failed to create one */

    if (deviceData.unsupportedCaps.has(NoEGLImage))
        return {};

    /* Attempt to create one */

    if (m_dmaInfo.has_value())
        deviceData.eglImage = REGLImage::MakeFromDMA(m_dmaInfo.value(), device);

    if (!deviceData.eglImage)
        deviceData.unsupportedCaps.add(NoEGLImage);

    return deviceData.eglImage;
}

std::shared_ptr<RGLImage> RGLImage::Make(SkISize size, const RDRMFormat &format, const RImageConstraints *constraints) noexcept
{
    std::lock_guard<std::recursive_mutex> lock(mutex);
    std::shared_ptr<RGLImage> image { MakeWithGBMStorage(size, format, constraints) };

    if (!image)
        image = MakeWithNativeStorage(size, format, constraints);

    return image;
}

std::shared_ptr<RGLImage> RGLImage::BorrowFramebuffer(const RGLFramebufferInfo &info, RGLDevice *allocator) noexcept
{
    std::lock_guard<std::recursive_mutex> lock(mutex);

    const RFormatInfo *formatInfo;
    SkAlphaType alphaType;

    auto core { ValidateMake(info.size, info.format, kUnknown_SkAlphaType, &allocator, &formatInfo, &alphaType) };

    if (!core) return {};

    // TODO: Validate

    auto image { std::shared_ptr<RGLImage>(new RGLImage(core, allocator, info.size, formatInfo, alphaType, DRM_FORMAT_MOD_INVALID)) };
    image->m_self = image;
    auto *contextData { static_cast<ContextData*>(image->m_contextDataManager->getData(allocator)) };
    contextData->glFb = info.id;
    contextData->fbOwnership = CZOwn::Borrow;
    return image;
}

std::shared_ptr<RGLImage> RGLImage::FromDMA(const RDMABufferInfo &info, CZOwn ownership, const RImageConstraints *constraints) noexcept
{
    std::lock_guard<std::recursive_mutex> lock(mutex);

    std::optional<RDMABufferInfo> infoCopy;

    if (ownership == CZOwn::Borrow)
    {
        if (!info.isValid())
            return {};

        infoCopy = info;
    }
    else
    {
        infoCopy = info.dup();

        if (!infoCopy.has_value())
            return {};
    }

    const RFormatInfo *formatInfo;
    SkAlphaType alphaType;
    RGLDevice *allocator { nullptr };

    if (constraints)
        allocator = (RGLDevice*)constraints->allocator;

    const auto size { SkISize::Make(info.width, info.height) };

    auto core { ValidateMake(size, info.format, kUnknown_SkAlphaType, &allocator, &formatInfo, &alphaType) };
    if (!core) return {};

    auto image { std::shared_ptr<RGLImage>(new RGLImage(core, allocator, size, formatInfo, alphaType, info.modifier)) };
    image->m_self = image;
    image->m_dmaInfoOwn = CZOwn::Own;
    image->m_dmaInfo = infoCopy;

    if (!ValidateConstraints(image, constraints))
        return {};

    return image;
}

std::shared_ptr<RGBMBo> RGLImage::gbmBo(RDevice *device) const noexcept
{
    std::lock_guard<std::recursive_mutex> lock(mutex);

    if (!device)
        device = core()->mainDevice();

    auto *dev { static_cast<RGLDevice*>(device) };
    auto &data { m_devicesMap[dev] };
    return data.gbmBo;
}

std::shared_ptr<RDRMFramebuffer> RGLImage::drmFb(RDevice *device) const noexcept
{
    std::lock_guard<std::recursive_mutex> lock(mutex);

    if (!device)
        device = core()->mainDevice();

    auto &data { m_devicesMap[device->asGL()] };

    /* Already has one */

    if (data.drmFb)
        return data.drmFb;

    /* Already failed before */

    if (data.unsupportedCaps.has(NoDRMFb))
        return {};

    /* Attempt to create one */

    data.drmFb = RDRMFramebuffer::MakeFromGBMBo(gbmBo(device));

    if (!data.drmFb && m_dmaInfo.has_value())
        data.drmFb = RDRMFramebuffer::MakeFromDMA(m_dmaInfo.value());

    if (!data.drmFb)
        data.unsupportedCaps.add(NoDRMFb);

    return data.drmFb;
}

sk_sp<SkImage> RGLImage::skImage(RDevice *device) const noexcept
{
    std::lock_guard<std::recursive_mutex> lock(mutex);

    if (!device)
        device = core()->mainDevice();

    auto *contextData { static_cast<ContextData*>(m_contextDataManager->getData(device->asGL())) };

    /* Already has one */

    if (contextData->skImage)
        return contextData->skImage;

    auto &deviceData { m_devicesMap[device->asGL()] };

    /* Already failed before */

    if (deviceData.unsupportedCaps.has(NoSkImage))
        return {};

    auto current { RGLMakeCurrent::FromDevice(device->asGL(), true) };

    /* Attempt to create one */

    auto skContext { device->asGL()->skContext() };

    if (!skContext)
    {
        deviceData.unsupportedCaps.add(NoSkImage);
        return {};
    }

    auto tex { texture((RGLDevice*)device) };

    if (tex.id == 0)
    {
        deviceData.unsupportedCaps.add(NoSkImage);
        return {};
    }

    auto skFormat { formatInfo().alpha ? kRGBA_8888_SkColorType : kRGB_888x_SkColorType };
    auto glFormat { formatInfo().alpha ? GL_RGBA8_OES : GL_RGB8_OES };

    GrGLTextureInfo skTextureInfo {};
    skTextureInfo.fFormat = glFormat;
    skTextureInfo.fID = tex.id;
    skTextureInfo.fTarget = tex.target;

    GrBackendTexture skTexture;
    skTexture = GrBackendTextures::MakeGL(
        size().width(),
        size().height(),
        skgpu::Mipmapped::kNo,
        skTextureInfo);

    contextData->skImage = SkImages::BorrowTextureFrom(
        skContext.get(),
        skTexture,
        GrSurfaceOrigin::kTopLeft_GrSurfaceOrigin,
        skFormat,
        alphaType(),
        skSRGB,
        nullptr,
        nullptr);

    if (!contextData->skImage)
        deviceData.unsupportedCaps.add(NoSkImage);

    return contextData->skImage;
}

sk_sp<SkSurface> RGLImage::skSurface(RDevice *device) const noexcept
{
    std::lock_guard<std::recursive_mutex> lock(mutex);

    if (!device)
        device = core()->mainDevice();

    /* Already has one */

    auto *contextData { static_cast<ContextData*>(m_contextDataManager->getData(device->asGL())) };

    if (contextData->skSurface)
        return contextData->skSurface;

    auto &deviceData { m_devicesMap[device->asGL()] };

    /* Already failed before */

    if (deviceData.unsupportedCaps.has(NoSkSurface))
        return {};

    auto current { RGLMakeCurrent::FromDevice(device->asGL(), true) };

    /* Attempt to create one */

    auto skContext { device->asGL()->skContext() };

    if (!skContext)
    {
        deviceData.unsupportedCaps.add(NoSkSurface);
        return {};
    }

    auto fb { glFb((RGLDevice*)device) };

    if (!fb.has_value())
    {
        deviceData.unsupportedCaps.add(NoSkSurface);
        return {};
    }

    auto skFormat { formatInfo().alpha ? kRGBA_8888_SkColorType : kRGB_888x_SkColorType };
    auto glFormat { formatInfo().alpha ? GL_RGBA8_OES : GL_RGB8_OES };

    const GrGLFramebufferInfo fbInfo
    {
        .fFBOID = fb.value(),
        .fFormat = (GrGLenum)glFormat
    };

    const GrBackendRenderTarget backendTarget = GrBackendRenderTargets::MakeGL(
        size().width(),
        size().height(),
        0, 0,
        fbInfo);

    // TODO: Add pixel geometry to constructor
    static SkSurfaceProps skSurfaceProps(0, kUnknown_SkPixelGeometry);

    contextData->skSurface = SkSurfaces::WrapBackendRenderTarget(
        skContext.get(),
        backendTarget,
        fbInfo.fFBOID == 0 ? GrSurfaceOrigin::kBottomLeft_GrSurfaceOrigin : GrSurfaceOrigin::kTopLeft_GrSurfaceOrigin,
        skFormat,
        skSRGB,
        &skSurfaceProps);

    if (!contextData->skSurface)
        deviceData.unsupportedCaps.add(NoSkSurface);

    return contextData->skSurface;
}

CZBitset<RImageCap> RGLImage::checkDeviceCaps(CZBitset<RImageCap> caps, RDevice *device) const noexcept
{
    std::lock_guard<std::recursive_mutex> lock(mutex);

    if (!device)
        device = m_core->mainDevice();

    CZBitset<RImageCap> out {};

    if (caps.has(RImageCap_Src))
        out.setFlag(RImageCap_Src, texture(device->asGL()).id != 0);

    if (caps.has(RImageCap_Dst))
        out.setFlag(RImageCap_Dst, glFb(device->asGL()).has_value());

    if (caps.has(RImageCap_SkImage))
        out.setFlag(RImageCap_SkImage, skImage(device->asGL()) != nullptr);

    if (caps.has(RImageCap_SkSurface))
        out.setFlag(RImageCap_SkSurface, skSurface(device->asGL()) != nullptr);

    if (caps.has(RImageCap_DRMFb))
        out.setFlag(RImageCap_DRMFb, drmFb(device) != nullptr);

    if (caps.has(RImageCap_GBMBo))
        out.setFlag(RImageCap_GBMBo, gbmBo(device) != nullptr);

    return out;
}

bool RGLImage::writePixels(const RPixelBufferRegion &region) noexcept
{
    std::lock_guard<std::recursive_mutex> lock(mutex);

    // TODO: Implement blitting as fallback

    if (!region.pixels)
        return false;

    if (region.region.isEmpty())
        return true;

    if (!writeFormats().contains(region.format))
        return false;

    if (writePixelsGBMMapWrite(region))
    {
        m_writeSerial++;
        return true;
    }

    if (writePixelsNative(region))
    {
        m_writeSerial++;
        return true;
    }

    return false;
}

bool RGLImage::readPixels(const RPixelBufferRegion &region) noexcept
{
    if (region.region.isEmpty()) return true;

    if (!readFormats().contains(region.format))
    {
        RLog(CZError, CZLN, "Unsupported read format: {}", RDRMFormat::FormatName(region.format));
        return false;
    }

    const auto bounds { region.region.getBounds().makeOffset(region.offset) };

    if (bounds.fLeft < 0 || bounds .fTop <  0 || bounds.fRight > size().width() || bounds.fBottom > size().height())
    {
        RLog(CZError, CZLN, "Invalid region + offset");
        return false;
    }

    if (readPixelsGBMmapRead(region))
        return true;

    if (readPixelsNative(region))
        return true;

    RLog(CZDebug, CZLN, "readPixels failed src format: {}, dst format: {}", RDRMFormat::FormatName(formatInfo().format), RDRMFormat::FormatName(region.format));
    return false;
}

std::shared_ptr<RGLImage> RGLImage::MakeWithGBMStorage(SkISize size, const RDRMFormat &format, const RImageConstraints *constraints) noexcept
{
    std::lock_guard<std::recursive_mutex> lock(mutex);
    const RFormatInfo *formatInfo;
    SkAlphaType alphaType;
    RGLDevice *allocator { nullptr };

    if (constraints)
        allocator = (RGLDevice*)constraints->allocator;

    auto core { ValidateMake(size, format.format(), kUnknown_SkAlphaType, &allocator, &formatInfo, &alphaType) };

    if (!core) return {};

    if (!allocator->gbmDevice())
    {
        allocator->log(CZDebug, CZLN, "Missing gbm_device");
        return {};
    }

    GlobalDeviceData data {};
    data.device = allocator;
    data.gbmBo = RGBMBo::Make(size, format, allocator);

    if (!data.gbmBo)
    {
        allocator->log(CZDebug, CZLN, "Failed to create gbm_bo");
        return {};
    }

    auto image { std::shared_ptr<RGLImage>(new RGLImage(core, allocator, size, formatInfo, alphaType, data.gbmBo->modifier())) };
    image->m_dmaInfo = data.gbmBo->dmaInfo();
    image->m_pf.add(PFStorageGBM);
    image->m_self = image;
    image->m_devicesMap.emplace(allocator, data);
    image->assignReadWriteFormats();

    if (!ValidateConstraints(image, constraints))
        return {};

    return image;
}

std::shared_ptr<RGLImage> RGLImage::MakeWithNativeStorage(SkISize size, const RDRMFormat &format, const RImageConstraints *constraints) noexcept
{
    std::lock_guard<std::recursive_mutex> lock(mutex);

    if (!format.modifiers().contains(DRM_FORMAT_MOD_INVALID))
        return {};

    const RFormatInfo *formatInfo;
    SkAlphaType alphaType;
    RGLDevice *allocator { nullptr };

    if (constraints)
        allocator = (RGLDevice*)constraints->allocator;

    auto core { ValidateMake(size, format.format(), kUnknown_SkAlphaType, &allocator, &formatInfo, &alphaType) };

    if (!core) return {};

    if (constraints)
    {
        for (const auto &dev : constraints->caps)
            if (dev.first != allocator && dev.second.get() != 0)
                return {};

        allocator = (RGLDevice*)constraints->allocator;
    }

    if (formatInfo->pixelsPerBlock() != 1)
    {
        allocator->log(CZDebug, CZLN, "Block formats are not supported: {}", RDRMFormat::FormatName(format.format()));
        return {};
    }

    const RGLFormat *glFormat { RGLFormat::FromDRM(format.format()) };

    if (!glFormat)
    {
        allocator->log(CZError, CZLN, "Failed to find GL equivalent for format: {}", RDRMFormat::FormatName(format.format()));
        return {};
    }

    if (glFormat->format != GL_RGBA && glFormat->format != GL_RGB)
    {
        if (glFormat->format != GL_BGRA_EXT || !allocator->glExtensions().EXT_texture_format_BGRA8888)
            return {};
    }

    auto image { std::shared_ptr<RGLImage>(new RGLImage(core, allocator, size, formatInfo, alphaType, DRM_FORMAT_MOD_INVALID)) };
    image->m_pf.add(PFStorageNative);
    image->m_self = image;

    const auto current { RGLMakeCurrent::FromDevice(allocator, false) };
    GlobalDeviceData &data { image->m_devicesMap.emplace(allocator, GlobalDeviceData{}).first->second };
    data.textureOwnership = CZOwn::Own;
    data.device = allocator;
    data.texture.target = GL_TEXTURE_2D;
    glGenTextures(1, &data.texture.id);
    glBindTexture(data.texture.target, data.texture.id);
    glTexImage2D(data.texture.target, 0, glFormat->internalFormat, size.width(), size.height(), 0, glFormat->format, glFormat->type, NULL);
    glBindTexture(data.texture.target, 0);
    image->assignReadWriteFormats();

    if (!ValidateConstraints(image, constraints))
        return {};

    return image;
}

bool RGLImage::writePixelsGBMMapWrite(const RPixelBufferRegion &region) noexcept
{
    std::lock_guard<std::recursive_mutex> lock(mutex);

    if (region.format != formatInfo().format)
        return false;

    /* Try map write with the allocator first */

    auto *dev { allocator() };
    auto bo { gbmBo(dev) };

    if (!bo || !bo->supportsMapWrite())
    {
        bo.reset();

        /* Check if another device supports it */

        for (auto *device : m_core->devices())
        {
            if (device == dev)
                continue;

            auto found = gbmBo(device);

            if (found && found->supportsMapWrite())
            {
                bo = found;
                break;
            }
        }

        if (!bo)
        {
            RLog(CZError, CZLN, "Failed to write pixels using gbm_bo_map");
            return false;
        }
    }

    UInt32 stride;
    void *mapData {};

    auto *dst { static_cast<UInt8*>(gbm_bo_map(bo->bo(), 0, 0, size().width(), size().height(), GBM_BO_TRANSFER_WRITE, &stride, &mapData)) };

    if (!dst) return false;

    const auto bpb { formatInfo().bytesPerBlock };

    SkRegion::Iterator iter(region.region);

    while (!iter.done())
    {
        const SkIRect &rect { iter.rect() };

        // Source starting point in pixels (apply offset)
        const auto srcX { rect.left() + region.offset.x() };
        const auto srcY { rect.top() + region.offset.y() };

        // Destination starting point in pixels
        const auto dstX { rect.left() };
        const auto dstY { rect.top() };

        const auto width { rect.width() };
        const auto height { rect.height() };

        for (int row = 0; row < height; row++)
        {
            auto *srcPtr = region.pixels + ((srcY + row) * region.stride) + (srcX * bpb);
            auto *dstPtr = dst + ((dstY + row) * stride) + (dstX * bpb);
            std::memcpy(dstPtr, srcPtr, width * bpb);
        }

        iter.next();
    }

    gbm_bo_unmap(bo->bo(), mapData);
    return true;
}

bool RGLImage::writePixelsNative(const RPixelBufferRegion &region) noexcept
{
    std::lock_guard<std::recursive_mutex> lock(mutex);

    if (!m_pf.has(PFStorageNative))
        return false;

    const auto *formatInfo { RDRMFormat::GetInfo(region.format) };

    if (formatInfo->pixelsPerBlock() != 1)
    {
        allocator()->log(CZError, CZLN, "Block formats are not supported: {}", RDRMFormat::FormatName(region.format));
        return false;
    }

    /*
    if (!formatInfo->validateStride(params.size.width(), params.stride))
    {
        allocator->log(CZError, CZLN, "Invalid stride {} for width {} and alignment {}", params.stride, params.size.width(), formatInfo->bytesPerBlock);
        return {};
    }*/

    const RGLFormat *glFormat { RGLFormat::FromDRM(region.format) };

    if (!glFormat)
    {
        allocator()->log(CZError, CZLN, "Failed to find GL equivalent for format: {}", RDRMFormat::FormatName(region.format));
        return false;
    }

    const auto current { RGLMakeCurrent::FromDevice(allocator(), false) };
    const auto tex { texture(allocator()) };
    const auto bpb { formatInfo->bytesPerBlock };

    glBindTexture(tex.target, tex.id);

    if (region.stride % bpb != 0) return false;

    const GLint unpackRowLength = region.stride / bpb;

    glPixelStorei(GL_UNPACK_ROW_LENGTH_EXT, unpackRowLength);

    SkRegion::Iterator iter { region.region };

    while (!iter.done())
    {
        const auto &rect { iter.rect() };

        // Source starting point in pixels (apply offset)
        const auto srcX { rect.left() + region.offset.x() };
        const auto srcY { rect.top() + region.offset.y() };

        // Destination starting point in pixels
        const auto dstX { rect.left() };
        const auto dstY { rect.top() };

        const auto width { rect.width() };
        const auto height { rect.height() };

        const auto* srcPtr = region.pixels + (srcY * region.stride) + (srcX * bpb);
        glTexSubImage2D(tex.target, 0, dstX, dstY, width, height, glFormat->format, glFormat->type, srcPtr);
        iter.next();
    }

    glPixelStorei(GL_UNPACK_ROW_LENGTH_EXT, 0);
    glBindTexture(tex.target, 0);
    setWriteSync(RSync::Make(allocator()));
    return true;
}

bool RGLImage::readPixelsGBMmapRead(const RPixelBufferRegion &region) noexcept
{
    if (region.format != formatInfo().format)
        return false;

    auto bo { gbmBo(allocator()) };

    if (!bo || !bo->supportsMapRead())
    {
        for (auto *dev : m_core->devices())
        {
            if (dev == allocator())
                continue;

            bo = gbmBo(dev);

            if (bo && bo->supportsMapRead())
                break;
        }
    }

    if (!bo || !bo->supportsMapRead())
        return false;

    const auto bpb { formatInfo().bytesPerBlock };
    UInt32 srcStride;
    void *mapData {};
    UInt8 *src = (UInt8*)gbm_bo_map(bo->bo(), 0, 0, size().width(), size().height(), GBM_BO_TRANSFER_READ, &srcStride, &mapData);

    SkRegion::Iterator iter (region.region);
    while (!iter.done())
    {
        SkIRect r = iter.rect();

        int srcX = r.x() + region.offset.x();
        int srcY = r.y() + region.offset.y();
        int width = r.width();
        int height = r.height();

        if (srcX < 0 || srcY < 0 || width <= 0 || height <= 0)
        {
            iter.next();
            continue;
        }

        for (int row = 0; row < height; ++row)
        {
            const UInt8* srcRow = src + (srcY + row) * srcStride + srcX * bpb;
            UInt8* dstRow = region.pixels + (r.y() + row) * region.stride + r.x() * bpb;
            std::memcpy(dstRow, srcRow, width * bpb);
        }

        iter.next();
    }

    gbm_bo_unmap(bo->bo(), mapData);

    return true;
}

bool RGLImage::readPixelsNative(const RPixelBufferRegion &region) noexcept
{
    RGLDevice *device { nullptr };
    std::optional<GLuint> fb;
    GLenum glFormat { GL_RGBA };

    if (region.format == DRM_FORMAT_XRGB8888 || region.format == DRM_FORMAT_ARGB8888)
        glFormat = GL_BGRA_EXT;

    for (RDevice *dev : m_core->devices())
    {
        if (glFormat == GL_BGRA_EXT && !dev->asGL()->glExtensions().EXT_read_format_bgra)
            continue;

        fb = glFb(dev->asGL());

        if (fb.has_value())
        {
            device = dev->asGL();
            break;
        }
    }

    if (!device)
        return false;

    if (writeSync())
        writeSync()->cpuWait();

    const auto current { RGLMakeCurrent::FromDevice(device, true) };
    glBindFramebuffer(GL_FRAMEBUFFER, fb.value());

    // Loop through each rect in the region
    SkRegion::Iterator iter(region.region);

    if (fb.value() == 0)
    {
        while (!iter.done())
        {
            SkIRect r = iter.rect();

            int srcX = r.x() + region.offset.x();
            int srcY = r.y() + region.offset.y();
            int width = r.width();
            int height = r.height();

            UInt8 *dst = region.pixels + r.y() * region.stride + r.x() * 4;

            for (int row = 0; row < height; ++row)
                glReadPixels(srcX, size().height() - (srcY + row) - 1, width, 1, glFormat, GL_UNSIGNED_BYTE, dst + row * region.stride);

            iter.next();
        }
    }
    else
    {
        while (!iter.done())
        {
            SkIRect r = iter.rect();

            int srcX = r.x() + region.offset.x();
            int srcY = r.y() + region.offset.y();
            int width = r.width();
            int height = r.height();

            UInt8 *dst = region.pixels + r.y() * region.stride + r.x() * 4;

            for (int row = 0; row < height; ++row)
                glReadPixels(srcX, srcY + row, width, 1, glFormat, GL_UNSIGNED_BYTE, dst + row * region.stride);

            iter.next();
        }
    }

    setWriteSync(RSync::Make(device));
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
}

void RGLImage::assignReadWriteFormats() noexcept
{
    std::lock_guard<std::recursive_mutex> lock(mutex);

    if (m_pf.has(PFStorageNative))
    {
        m_writeFormats.emplace(DRM_FORMAT_ABGR8888);
        m_writeFormats.emplace(DRM_FORMAT_XBGR8888);

        if (allocator()->glExtensions().EXT_texture_format_BGRA8888)
        {
            m_readFormats.emplace(DRM_FORMAT_ARGB8888);
            m_readFormats.emplace(DRM_FORMAT_XRGB8888);
        }
    }

    for (auto *dev : m_core->devices())
    {
        auto *glDev { static_cast<RGLDevice*>(dev) };

        if (glFb(glDev).has_value())
        {
            m_readFormats.emplace(DRM_FORMAT_ABGR8888);
            m_readFormats.emplace(DRM_FORMAT_XBGR8888);

            if (glDev->glExtensions().EXT_read_format_bgra)
            {
                m_readFormats.emplace(DRM_FORMAT_ARGB8888);
                m_readFormats.emplace(DRM_FORMAT_XRGB8888);
            }
        }

        auto bo { gbmBo(glDev) };

        if (bo)
        {
            if (bo->supportsMapRead())
                m_readFormats.emplace(formatInfo().format);

            if (bo->supportsMapWrite())
                m_writeFormats.emplace(formatInfo().format);
        }
    }
}

RGLImage::RGLImage(std::shared_ptr<RCore> core, RDevice *device, SkISize size, const RFormatInfo *formatInfo, SkAlphaType alphaType, RModifier modifier) noexcept
    : RImage(core, (RDevice*)device, size, formatInfo, alphaType, modifier)
{
    std::lock_guard<std::recursive_mutex> lock(mutex);

    m_contextDataManager = RGLContextDataManager::Make([](RGLDevice *device) -> RGLContextData* {
        return new ContextData(device);
    });
}

RGLImage::GlobalDeviceDataMap::~GlobalDeviceDataMap() noexcept
{
    std::lock_guard<std::recursive_mutex> lock(mutex);

    while (!empty())
    {
        const auto &it { begin() };
        auto &data { it->second };

        if (data.textureOwnership == CZOwn::Own && data.texture.id != 0)
        {
            auto current { RGLMakeCurrent::FromDevice(data.device, false) };
            glDeleteTextures(1, &data.texture.id);
        }

        erase(it);
    }
}

RGLImage::ContextData::~ContextData() noexcept
{
    std::lock_guard<std::recursive_mutex> lock(mutex);

    if (fbOwnership == CZOwn::Own && glFb.has_value())
        glDeleteFramebuffers(1, &glFb.value());
}
