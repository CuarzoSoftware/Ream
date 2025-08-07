#include <CZ/Ream/RLog.h>
#include <CZ/Ream/GBM/RGBMBo.h>
#include <CZ/Ream/RCore.h>
#include <CZ/Ream/RDevice.h>
#include <CZ/Ream/DRM/RDRMFormat.h>
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
    RDMABufferInfo dmaInfo {};
    dmaInfo.format = format.format();
    dmaInfo.width = size.width();
    dmaInfo.height = size.height();

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
        dmaInfo.modifier = gbm_bo_get_modifier(bo);
    }
    else
    {
        if (!bo && format.modifiers().contains(DRM_FORMAT_MOD_LINEAR))
        {
            dmaInfo.modifier = DRM_FORMAT_MOD_LINEAR;
            hasModifier = false;
            bo = gbm_bo_create(allocator->gbmDevice(), size.width(), size.height(), format.format(), GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING | GBM_BO_USE_LINEAR);
        }

        if (format.modifiers().contains(DRM_FORMAT_MOD_INVALID))
        {
            dmaInfo.modifier = DRM_FORMAT_MOD_INVALID;
            hasModifier = false;
            bo = gbm_bo_create(allocator->gbmDevice(), size.width(), size.height(), format.format(), GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
        }
    }

    if (!bo)
    {
        allocator->log(CZError, CZLN, "Failed to create gbm_bo");
        return {};
    }

    dmaInfo.planeCount = gbm_bo_get_plane_count(bo);

    for (int i = 0; i < dmaInfo.planeCount; i++)
    {
        dmaInfo.stride[i] = gbm_bo_get_stride_for_plane(bo, i);
        dmaInfo.offset[i] = gbm_bo_get_offset(bo, i);
        dmaInfo.fd[i] = gbm_bo_get_fd_for_plane(bo, i);

        if (dmaInfo.fd[i] < 0)
        {
            for (int j = 0; j < 4; j++)
                if (dmaInfo.fd[j] >= 0)
                    close(dmaInfo.fd[j]);

            gbm_bo_destroy(bo);
            allocator->log(CZError, CZLN, "Failed to export gbm_bo");
            return {};
        }
    }

    return std::shared_ptr<RGBMBo>(new RGBMBo(core, allocator, bo, CZOwn::Own, hasModifier, dmaInfo));
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

    auto dmaDup { dmaInfo.dup() };

    if (!dmaDup.has_value())
    {
        RLog(CZError, CZLN, "Failed to dup DMA info");
        gbm_bo_destroy(bo);
        return {};
    }

    return std::shared_ptr<RGBMBo>(new RGBMBo(core, importer, bo, CZOwn::Own, hasModifier, dmaDup.value()));
}

gbm_bo_handle RGBMBo::planeHandle(int planeIndex) const noexcept
{
    return gbm_bo_get_handle_for_plane(m_bo, planeIndex);
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

RGBMBo::~RGBMBo() noexcept
{
    for (int i = 0; i < planeCount(); i++)
        close(m_dmaInfo.fd[i]);

    if (m_ownership == CZOwn::Own)
        gbm_bo_destroy(m_bo);
}

RGBMBo::RGBMBo(std::shared_ptr<RCore> core, RDevice *allocator, gbm_bo *bo, CZOwn ownership, bool hasModifier, const RDMABufferInfo &dmaInfo) noexcept :
    m_bo(bo), m_dmaInfo(dmaInfo), m_ownership(ownership), m_allocator(allocator), m_core(core), m_hasModifier(hasModifier)
{
    assert(core);
    assert(bo);
    assert(allocator);
    assert(dmaInfo.planeCount > 0 && dmaInfo.planeCount < 4);
    assert(dmaInfo.width > 0 && dmaInfo.height > 0);

    for (int i = 0; i < dmaInfo.planeCount; i++)
        assert(dmaInfo.fd[i] >= 0);

    if (hasModifier)
        assert(dmaInfo.modifier != DRM_FORMAT_MOD_INVALID);
    else
        assert(dmaInfo.modifier == DRM_FORMAT_MOD_INVALID || dmaInfo.modifier == DRM_FORMAT_MOD_LINEAR);
}
