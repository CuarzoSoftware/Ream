#include <CZ/Ream/SK/RSKFormat.h>
#include <CZ/Ream/RS/RRSImage.h>
#include <CZ/Ream/RS/RRSDevice.h>
#include <CZ/Ream/RDMABufferInfo.h>
#include <CZ/Ream/RCore.h>
#include <CZ/skia/core/SkImage.h>
#include <CZ/skia/core/SkPixmap.h>
#include <CZ/skia/core/SkSurface.h>
#include <cstring>
#include <sys/mman.h>

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

    if (!m_shm)
        return false;

    auto *dst { pixels() };
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
    auto *src { pixels() };

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

int RRSImage::resize(SkISize size) noexcept
{
    if (!m_shm)
        return -1;

    if (size.isEmpty())
    {
        RLog(CZError, CZLN, "Invalid size {}x{}", size.width(), size.height());
        return -1;
    }

    int ret { 0 };

    if (size == m_size)
        return ret;

    const auto newStride { size.width() * formatInfo().bytesPerBlock };
    const size_t newByteSize { size.height() * newStride };

    m_stride = newStride;
    m_size = size;
    m_skSurface.reset();
    m_skImage.reset();

    if (newByteSize > m_shm->size())
    {
        ret = 1;
        assert(m_shm->resize(newByteSize));
    }

    auto data { SkData::MakeWithoutCopy(m_shm->map(), m_shm->size()) };
    assert(data);

    const auto skFormat { RSKFormat::FromDRM(formatInfo().format) };
    auto info { SkImageInfo::Make(size, skFormat, kPremul_SkAlphaType) };

    m_skImage = SkImages::RasterFromData(info, data, newStride);
    assert(m_skImage);

    m_skSurface = SkSurfaces::WrapPixels(info, m_shm->map(), newStride);
    assert(m_skSurface);
    return ret;
}

RRSImage::RRSImage(std::shared_ptr<RCore> core, std::shared_ptr<CZSharedMemory> shm, sk_sp<SkImage> skImage, sk_sp<SkSurface> skSurface, RDevice *device,
                   SkISize size, size_t stride, const RFormatInfo *formatInfo, SkAlphaType alphaType, RModifier modifier) noexcept :
    RImage(core, device, size, formatInfo, alphaType, modifier),
    m_stride(stride), m_shm(shm), m_skImage(skImage), m_skSurface(skSurface)
{
    m_writeFormats.emplace(formatInfo->format);
    m_readFormats.emplace(formatInfo->format);
}

RRSImage::~RRSImage() noexcept
{
    if (m_dmaMap)
        munmap(m_dmaMap, m_dmaMapSize);
}

std::shared_ptr<RRSImage> RRSImage::FromDMA(const RDMABufferInfo &info, CZOwn ownership, const RImageConstraints *constraints) noexcept
{
    auto closeFds = [&]() {
        if (ownership == CZOwn::Own)
        {
            auto copy { info };
            copy.closeFds();
        }
    };

    auto core { RCore::Get() };
    RDevice *device { (constraints && constraints->allocator) ? constraints->allocator : (core ? core->mainDevice() : nullptr) };
    const auto *formatInfo { RDRMFormat::GetInfo(info.format) };
    const auto skFormat { RSKFormat::FromDRM(info.format) };

    if (!core || !device || !info.isValid() || info.planeCount != 1 || !formatInfo || skFormat == kUnknown_SkColorType ||
        (info.modifier != DRM_FORMAT_MOD_LINEAR && info.modifier != DRM_FORMAT_MOD_INVALID) ||
        info.stride[0] < (UInt32)info.width * formatInfo->bytesPerBlock)
    {
        RLog(CZDebug, CZLN, "RRSImage::FromDMA: unsupported buffer ({} {}x{}, modifier {:#x}, {} planes)",
             drmGetFormatName(info.format), info.width, info.height, info.modifier, info.planeCount);
        closeFds();
        return {};
    }

    const size_t pageSize { (size_t)getpagesize() };
    const size_t used { (size_t)info.offset[0] + (size_t)info.stride[0] * info.height };
    const size_t mapSize { (used + pageSize - 1) & ~(pageSize - 1) };
    void *map { mmap(nullptr, mapSize, PROT_READ, MAP_SHARED, info.fd[0], 0) };
    closeFds();

    if (map == MAP_FAILED)
    {
        RLog(CZError, CZLN, "RRSImage::FromDMA: mmap failed: {}", strerror(errno));
        return {};
    }

    const SkAlphaType alphaType { formatInfo->alpha ? kPremul_SkAlphaType : kOpaque_SkAlphaType };
    UInt8 *first { (UInt8*)map + info.offset[0] };
    const SkPixmap pixmap { SkImageInfo::Make(info.width, info.height, skFormat, alphaType), first, info.stride[0] };
    auto skImage { SkImages::RasterFromPixmap(pixmap, nullptr, nullptr) };

    if (!skImage)
    {
        munmap(map, mapSize);
        return {};
    }

    auto image { std::shared_ptr<RRSImage>(new RRSImage(core, nullptr, skImage, nullptr, device,
        SkISize(info.width, info.height), info.stride[0], formatInfo, alphaType, info.modifier)) };
    image->m_self = image;
    image->m_dmaMap = map;
    image->m_dmaMapSize = mapSize;
    image->m_dmaPixels = first;
    image->m_writeFormats.clear();
    return image;
}
