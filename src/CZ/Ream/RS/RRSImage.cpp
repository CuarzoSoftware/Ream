#include <CZ/Ream/SK/RSKFormat.h>
#include <CZ/Ream/RS/RRSImage.h>
#include <CZ/Ream/RS/RRSDevice.h>
#include <CZ/Ream/RCore.h>
#include <CZ/skia/core/SkImage.h>
#include <CZ/skia/core/SkSurface.h>

using namespace CZ;

std::shared_ptr<RRSImage> RRSImage::Make(SkISize size, const RDRMFormat &format, const RImageConstraints *constraints) noexcept
{
    auto core { RCore::Get() };

    if (!core)
    {
        RLog(CZError, CZLN, "Missing RCore");
        return {};
    }

    if (size.isEmpty())
    {
        RLog(CZError, CZLN, "Invalid size {}x{}", size.width(), size.height());
        return {};
    }

    RDevice *device { core->mainDevice() };

    if (constraints)
    {
        if (constraints->allocator)
            device = constraints->allocator;

        for (const auto &cap : constraints->caps)
        {
            if (!cap.first)
            {
                RLog(CZError, CZLN, "Invalid RImageConstraints caps (nullptr device key)");
                return {};
            }

            if (cap.second.has(RImageCap_DRMFb))
            {
                cap.first->log(CZError, CZLN, "Missing required device cap RImageCap_DRMFb");
                return {};
            }

            if (cap.second.has(RImageCap_GBMBo))
            {
                cap.first->log(CZError, CZLN, "Missing required device cap RImageCap_GBMBo");
                return {};
            }
        }

        if (!constraints->readFormats.empty() && !constraints->readFormats.contains(format.format()))
        {
            RLog(CZError, CZLN, "Missing requested read format");
            return {};
        }

        if (!constraints->writeFormats.empty() && !constraints->writeFormats.contains(format.format()))
        {
            RLog(CZError, CZLN, "Missing requested write format");
            return {};
        }
    }

    const auto *formatInfo { RDRMFormat::GetInfo(format.format()) };
    const auto skFormat { RSKFormat::FromDRM(format.format()) };

    if (!formatInfo || skFormat == kUnknown_SkColorType || !device->textureFormats().formats().contains(format.format()) ||
        !(format.modifiers().contains(DRM_FORMAT_MOD_LINEAR) || format.modifiers().contains(DRM_FORMAT_MOD_INVALID)))
    {
        RLog(CZError, CZLN, "Unsupported format {}", RDRMFormat::FormatName(format.format()));
        return {};
    }

    // Guaranteed: block size = 1x1
    const auto stride { size.width() * formatInfo->bytesPerBlock };
    const size_t bytes { size.height() * stride };

    auto shm { CZSharedMemory::Make(bytes) };

    if (!shm)
    {
        RLog(CZError, CZLN, "Failed to allocate SHM");
        return {};
    }

    auto info { SkImageInfo::Make(size, skFormat, kPremul_SkAlphaType) };
    auto data { SkData::MakeWithoutCopy(shm->map(), shm->size()) };

    if (!data)
    {
        RLog(CZError, CZLN, "Failed to wrap shm (SkData::MakeWithoutCopy)");
        return {};
    }

    auto skImage { SkImages::RasterFromData(info, data, stride) };

    if (!skImage)
    {
        RLog(CZError, CZLN, "Failed to create SkImage");
        return {};
    }

    auto skSurface { SkSurfaces::WrapPixels(info, shm->map(), stride) };

    if (!skSurface)
    {
        RLog(CZError, CZLN, "Failed to create SkSurface");
        return {};
    }

    auto image { std::shared_ptr<RRSImage>(new RRSImage(core, shm, skImage, skSurface, device, size, stride, formatInfo, kPremul_SkAlphaType, DRM_FORMAT_MOD_LINEAR)) };
    image->m_self = image;
    return image;
}

CZBitset<RImageCap> RRSImage::checkDeviceCaps(CZBitset<RImageCap> caps, RDevice *device) const noexcept
{
    CZ_UNUSED(device);
    caps.remove(RImageCap_DRMFb | RImageCap_GBMBo);
    return caps;
}

bool RRSImage::writePixels(const RPixelBufferRegion &region) noexcept
{
    if (region.format != formatInfo().format)
        return false;

    if (region.region.isEmpty())
        return true;

    auto *dst { (UInt8*)m_shm->map() };
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
            auto *dstPtr = dst + ((dstY + row) * m_stride) + (dstX * bpb);
            std::memcpy(dstPtr, srcPtr, width * bpb);
        }

        iter.next();
    }
    m_writeSerial++;
    return true;
}

bool RRSImage::readPixels(const RPixelBufferRegion &region) noexcept
{
    if (region.format != formatInfo().format)
        return false;

    const auto bpb { formatInfo().bytesPerBlock };
    auto *src { (UInt8*)m_shm->map() };

    SkRegion::Iterator iter (region.region);
    while (!iter.done())
    {
        SkIRect r = iter.rect();

        const auto srcX { r.x() + region.offset.x() };
        const auto srcY { r.y() + region.offset.y() };
        const auto width { r.width() };
        const auto height { r.height() };

        if (srcX < 0 || srcY < 0 || width <= 0 || height <= 0)
        {
            iter.next();
            continue;
        }

        for (int row = 0; row < height; ++row)
        {
            const UInt8* srcRow = src + (srcY + row) * m_stride + srcX * bpb;
            UInt8* dstRow = region.pixels + (r.y() + row) * region.stride + r.x() * bpb;
            std::memcpy(dstRow, srcRow, width * bpb);
        }

        iter.next();
    }

    return true;
}

RRSImage::RRSImage(std::shared_ptr<RCore> core, std::shared_ptr<CZSharedMemory> shm, sk_sp<SkImage> skImage, sk_sp<SkSurface> skSurface, RDevice *device,
                   SkISize size, size_t stride, const RFormatInfo *formatInfo, SkAlphaType alphaType, RModifier modifier) noexcept :
    RImage(core, device, size, formatInfo, alphaType, modifier),
    m_stride(stride), m_shm(shm), m_skImage(skImage), m_skSurface(skSurface)
{
    m_writeFormats.emplace(formatInfo->format);
    m_readFormats.emplace(formatInfo->format);
}

