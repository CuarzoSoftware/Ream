#include <CZ/Ream/DRM/RDumbBuffer.h>
#include <CZ/Ream/DRM/RDRMFramebuffer.h>
#include <CZ/Ream/RDevice.h>
#include <CZ/Ream/RCore.h>
#include <CZ/Ream/RLog.h>
#include <sys/mman.h>
#include <drm_fourcc.h>
#include <xf86drmMode.h>

using namespace CZ;

std::shared_ptr<RDumbBuffer> RDumbBuffer::Make(SkISize size, const RDRMFormat &format, RDevice *allocator) noexcept
{
    if (size.isEmpty())
    {
        RLog(CZError, CZLN, "Invalid size {}x{}", size.width(), size.height());
        return {};
    }

    auto core { RCore::Get() };

    if (!core)
    {
        RLog(CZError, CZLN, "Missing RCore");
        return {};
    }

    if (!allocator)
        allocator = core->mainDevice();

    if (!allocator->caps().DumbBuffer)
    {
        allocator->log(CZError, "Missing DumbBuffer cap");
        return {};
    }

    if (!format.modifiers().contains(DRM_FORMAT_MOD_INVALID) && !format.modifiers().contains(DRM_FORMAT_MOD_LINEAR))
    {
        allocator->log(CZError, "Dumb buffers can only have a DRM_FORMAT_MOD_INVALID or DRM_FORMAT_MOD_LINEAR modifier");
        return {};
    }

    const auto *formatInfo { RDRMFormat::GetInfo(format.format()) };

    if (!formatInfo)
    {
        RLog(CZError, CZLN, "Unsupported format {}", RDRMFormat::FormatName(format.format()));
        return {};
    }

    if (formatInfo->pixelsPerBlock() != 1)
    {
        RLog(CZError, CZLN, "Block format not supported: {}", RDRMFormat::FormatName(format.format()));
        return {};
    }

    UInt32 handle {};
    UInt32 stride {};
    UInt64 mapSize {};
    UInt64 offset {};
    UInt8 *data {};

    if (drmModeCreateDumbBuffer(allocator->drmFd(), size.width(), size.height(), formatInfo->bpp, 0, &handle, &stride, &mapSize) != 0)
    {
        allocator->log(CZError, "Failed to create dumb buffer");
        return {};
    }

    if (drmModeMapDumbBuffer(allocator->drmFd(), handle, &offset) != 0)
    {
        drmModeDestroyDumbBuffer(allocator->drmFd(), handle);
        allocator->log(CZError, "Failed to map dumb buffer");
        return {};
    }

    data = (UInt8*)mmap(NULL, mapSize, PROT_READ | PROT_WRITE, MAP_SHARED, allocator->drmFd(), offset);

    if (data == MAP_FAILED)
    {
        drmModeDestroyDumbBuffer(allocator->drmFd(), handle);
        allocator->log(CZError, "Failed to map dumb buffer");
        return {};
    }

    auto fb { RDRMFramebuffer::WrapHandle(size, stride, format.format(), DRM_FORMAT_MOD_LINEAR, handle, CZOwn::Borrow, allocator) };

    if (!fb)
    {
        munmap(data, mapSize);
        drmModeDestroyDumbBuffer(allocator->drmFd(), handle);
        allocator->log(CZError, "Failed to create DRM framebuffer");
        return {};
    }

    return std::shared_ptr<RDumbBuffer>(new RDumbBuffer(core, fb, size, formatInfo, allocator, handle, stride, mapSize, data));
}

RDumbBuffer::~RDumbBuffer() noexcept
{
    munmap(m_data, m_mapSize);
    drmModeDestroyDumbBuffer(allocator()->drmFd(), m_handle);
}

std::shared_ptr<RDRMFramebuffer> RDumbBuffer::fb() const noexcept
{
    return m_fb;
}

RDumbBuffer::RDumbBuffer(std::shared_ptr<RCore> core, std::shared_ptr<RDRMFramebuffer> fb, SkISize size, const RFormatInfo *formatInfo, RDevice *allocator, UInt32 handle, UInt32 stride, UInt64 mapSize, UInt8 *data) noexcept :
    m_core(core), m_fb(fb), m_size(size), m_formatInfo(formatInfo), m_allocator(allocator), m_handle(handle), m_stride(stride), m_mapSize(mapSize), m_data(data)
{}
