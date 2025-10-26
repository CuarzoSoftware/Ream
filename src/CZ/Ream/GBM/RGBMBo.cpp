#include <CZ/Ream/RLog.h>
#include <CZ/Ream/GBM/RGBMBo.h>
#include <CZ/Ream/RCore.h>
#include <CZ/Ream/RDevice.h>
#include <CZ/Ream/DRM/RDRMFormat.h>
#include <CZ/Ream/RResourceTracker.h>
#include <CZ/skia/core/SkSize.h>

#include <drm_fourcc.h>
#include <gbm.h>

using namespace CZ;

std::shared_ptr<RGBMBo> RGBMBo::Make(SkISize size, const RDRMFormat &format, RDevice *allocator) noexcept
{
    auto core { RCore::Get() };

    if (!core)
    {
        RLog(CZError, CZLN, "Missing RCore");
        return {};
    }

    if (!allocator)
    {
        assert(core->mainDevice());
        allocator = core->mainDevice();
    }

    if (!allocator->gbmDevice())
    {
        allocator->log(CZError, CZLN, "Missing gbm_device");
        return {};
    }

    if (size.isEmpty())
    {
        allocator->log(CZError, CZLN, "Invalid size ({}x{})", size.width(), size.height());
        return {};
    }

    if (format.modifiers().empty())
    {
        allocator->log(CZError, "The RDRMFormat has no modifiers");
        return {};
    }

    bool hasModifier;

    gbm_bo *bo { gbm_bo_create_with_modifiers(
        allocator->gbmDevice(),
        size.width(),
        size.height(),
        format.format(),
        std::to_address(format.modifiers().begin()),
        format.modifiers().size()) };

    if (bo)
    {
        hasModifier = true;
    }
    else
    {
        if (!bo && format.modifiers().contains(DRM_FORMAT_MOD_LINEAR))
        {
            hasModifier = false;
            bo = gbm_bo_create(allocator->gbmDevice(), size.width(), size.height(), format.format(), GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING | GBM_BO_USE_LINEAR);
        }

        if (format.modifiers().contains(DRM_FORMAT_MOD_INVALID))
        {
            hasModifier = false;
            bo = gbm_bo_create(allocator->gbmDevice(), size.width(), size.height(), format.format(), GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
        }
    }

    if (!bo)
    {
        allocator->log(CZError, CZLN, "Failed to create gbm_bo");
        return {};
    }

    return std::shared_ptr<RGBMBo>(new RGBMBo(core, allocator, bo, CZOwn::Own, hasModifier));
}

std::shared_ptr<RGBMBo> RGBMBo::MakeCursor(SkISize size, RFormat format, RDevice *allocator) noexcept
{
    auto core { RCore::Get() };

    if (!core)
    {
        RLog(CZError, CZLN, "Missing RCore");
        return {};
    }

    if (!allocator)
        allocator = core->mainDevice();

    if (!allocator->gbmDevice())
    {
        allocator->log(CZError, CZLN, "Missing gbm_device");
        return {};
    }

    if (size.isEmpty())
    {
        allocator->log(CZError, CZLN, "Invalid size ({}x{})", size.width(), size.height());
        return {};
    }

    auto *formatInfo { RDRMFormat::GetInfo(format) };

    if (!formatInfo)
    {
        allocator->log(CZError, CZLN, "Unsupported format: {}", RDRMFormat::FormatName(format));
        return {};
    }

    const bool hasModifier { false };

    gbm_bo *bo = gbm_bo_create(allocator->gbmDevice(), size.width(), size.height(), format, GBM_BO_USE_CURSOR | GBM_BO_USE_WRITE | GBM_BO_USE_LINEAR);

    if (!bo)
    {
        allocator->log(CZError, CZLN, "Failed to create gbm_bo");
        return {};
    }

    return std::shared_ptr<RGBMBo>(new RGBMBo(core, allocator, bo, CZOwn::Own, hasModifier));
}

std::shared_ptr<RGBMBo> RGBMBo::MakeFromDMA(const RDMABufferInfo &dmaInfo, RDevice *importer) noexcept
{
    if (!dmaInfo.isValid())
    {
        RLog(CZError, CZLN, "Invalid DMA info");
        return {};
    }

    auto core { RCore::Get() };

    if (!core)
    {
        RLog(CZError, CZLN, "Missing RCore");
        return {};
    }

    if (!importer)
    {
        assert(core->mainDevice());
        importer = core->mainDevice();
    }

    if (!importer->gbmDevice())
    {
        importer->log(CZError, CZLN, "Missing gbm_device");
        return {};
    }

    gbm_import_fd_modifier_data data {};
    data.width = dmaInfo.width;
    data.height = dmaInfo.height;
    data.modifier = dmaInfo.modifier;
    data.format = dmaInfo.format;
    data.num_fds = dmaInfo.planeCount;

    for (int i = 0; i < dmaInfo.planeCount; i++)
    {
        data.fds[i] = dmaInfo.fd[i];
        data.offsets[i] = dmaInfo.offset[i];
        data.strides[i] = dmaInfo.stride[i];
    }

    bool hasModifier;
    gbm_bo *bo {};

    if (dmaInfo.modifier != DRM_FORMAT_MOD_INVALID)
    {
        hasModifier = true;
        bo = gbm_bo_import(importer->gbmDevice(), GBM_BO_IMPORT_FD_MODIFIER, &data, GBM_BO_USE_RENDERING | GBM_BO_USE_SCANOUT);
    }

    if (!bo && dmaInfo.modifier == DRM_FORMAT_MOD_INVALID)
    {
        hasModifier = false;
        bo = gbm_bo_import(importer->gbmDevice(), GBM_BO_IMPORT_FD, &data, GBM_BO_USE_RENDERING | GBM_BO_USE_SCANOUT);
    }

    if (!bo && dmaInfo.modifier == DRM_FORMAT_MOD_LINEAR)
    {
        hasModifier = false;
        bo = gbm_bo_import(importer->gbmDevice(), GBM_BO_IMPORT_FD, &data, GBM_BO_USE_RENDERING | GBM_BO_USE_SCANOUT | GBM_BO_USE_LINEAR);
    }

    if (!bo)
    {
        importer->log(CZError, CZLN, "Failed create gbm_bo from DMA buffer");
        return {};
    }

    return std::shared_ptr<RGBMBo>(new RGBMBo(core, importer, bo, CZOwn::Own, hasModifier));
}

RModifier RGBMBo::modifier() const noexcept
{
    return gbm_bo_get_modifier(m_bo);
}

RFormat RGBMBo::format() const noexcept
{
    return gbm_bo_get_format(m_bo);
}

SkISize RGBMBo::size() const noexcept
{
    return SkISize(gbm_bo_get_width(m_bo), gbm_bo_get_height(m_bo));
}

int RGBMBo::planeCount() const noexcept
{
    return gbm_bo_get_plane_count(m_bo);
}

gbm_bo_handle RGBMBo::planeHandle(int planeIndex) const noexcept
{
    return gbm_bo_get_handle_for_plane(m_bo, planeIndex);
}

UInt32 RGBMBo::planeStride(int planeIndex) const noexcept
{
    return gbm_bo_get_stride_for_plane(m_bo, planeIndex);
}

UInt32 RGBMBo::planeOffset(int planeIndex) const noexcept
{
    return gbm_bo_get_offset(m_bo, planeIndex);
}

CZSpFd RGBMBo::planeFd(int planeIndex) const noexcept
{
    return CZSpFd(gbm_bo_get_fd_for_plane(m_bo, planeIndex));
}

bool RGBMBo::supportsMapRead() const noexcept
{
    if (m_supportsMapRead == 2) // 2 => not yet checked
    {
        void *mapData { nullptr };
        UInt32 stride;

        if (!gbm_bo_map(m_bo, 0, 0, 1, 1, GBM_BO_TRANSFER_READ, &stride, &mapData))
            m_supportsMapRead = 0;
        else
        {
            m_supportsMapRead = 1;
            gbm_bo_unmap(m_bo, mapData);
        }
    }

    return m_supportsMapRead == 1;
}

bool RGBMBo::supportsMapWrite() const noexcept
{
    if (m_supportsMapWrite == 2) // 2 => not yet checked
    {
        void *mapData { nullptr };
        UInt32 stride;

        if (!gbm_bo_map(m_bo, 0, 0, 1, 1, GBM_BO_TRANSFER_WRITE, &stride, &mapData))
            m_supportsMapWrite = 0;
        else
        {
            m_supportsMapWrite = 1;
            gbm_bo_unmap(m_bo, mapData);
        }
    }

    return m_supportsMapWrite == 1;
}

std::optional<RDMABufferInfo> RGBMBo::dmaExport() const noexcept
{
    RDMABufferInfo info {};
    info.format = format();
    info.modifier = modifier();
    info.width = size().width();
    info.height = size().height();
    info.planeCount = planeCount();

    for (int i = 0; i < planeCount(); i++)
    {
        info.stride[i] = planeStride(i);
        info.offset[i] = planeOffset(i);
        info.fd[i] = planeFd(i).release();

        if (info.fd[i] < 0)
        {
            info.closeFds();
            return {};
        }
    }

    return info;
}

RGBMBo::~RGBMBo() noexcept
{
    RResourceTrackerSub(RGBMBoRes);
    if (m_ownership == CZOwn::Own)
        gbm_bo_destroy(m_bo);
}

RGBMBo::RGBMBo(std::shared_ptr<RCore> core, RDevice *allocator, gbm_bo *bo, CZOwn ownership, bool hasModifier) noexcept :
    m_bo(bo), m_ownership(ownership), m_allocator(allocator), m_core(core), m_hasModifier(hasModifier)
{
    RResourceTrackerAdd(RGBMBoRes);
}
