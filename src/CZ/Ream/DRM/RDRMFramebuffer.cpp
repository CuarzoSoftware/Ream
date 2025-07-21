#include <CZ/Ream/DRM/RDRMFormat.h>
#include <CZ/Ream/DRM/RDRMFramebuffer.h>
#include <CZ/Ream/RLog.h>
#include <CZ/Ream/GBM/RGBMBo.h>
#include <CZ/Ream/RCore.h>
#include <CZ/Ream/RDevice.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm_fourcc.h>
#include <unordered_set>

using namespace CZ;

static void closeHandles(int drmFd, UInt32 *handles, size_t n) noexcept
{
    std::unordered_set<UInt32> closedHandles;

    for (size_t i = 0; i < n; i++)
    {
        if (handles[i] == 0 || closedHandles.contains(handles[i]))
            continue;

        closedHandles.emplace(handles[i]);
        drmCloseBufferHandle(drmFd, handles[i]);
    }
}

std::shared_ptr<RDRMFramebuffer> RDRMFramebuffer::MakeFromGBMBo(std::shared_ptr<RGBMBo> bo) noexcept
{
    if (!bo)
    {
        RLog(CZError, CZLN, "Invalid RGBMBo");
        return {};
    }

    auto core { bo->core() };

    UInt32 id { 0 };
    bool hasModifier;
    bool pass { false };
    UInt32 handles[4] {};
    RModifier modifiers[4] {};
    RFormat format { bo->dmaInfo().format };
    RDevice *device { bo->allocator() };

    for (int i = 0; i < bo->planeCount(); i++)
    {
        handles[i] = bo->planeHandle(i).u32;
        modifiers[i] = bo->modifier();
    }

    if (device->caps().AddFb2Modifiers && (bo->hasModifier() || bo->modifier() == DRM_FORMAT_MOD_LINEAR))
    {
        if (drmModeAddFB2WithModifiers(
            device->drmFd(),
            bo->dmaInfo().width,
            bo->dmaInfo().height,
            format,
            handles,
            bo->dmaInfo().stride,
            bo->dmaInfo().offset,
            modifiers,
            &id,
            DRM_MODE_FB_MODIFIERS) == 0)
        {
            pass = true;
            hasModifier = true;
            goto end;
        }
    }

    if (!bo->hasModifier())
    {
        if (drmModeAddFB2(
            device->drmFd(),
            bo->dmaInfo().width,
            bo->dmaInfo().height,
            format,
            handles,
            bo->dmaInfo().stride,
            bo->dmaInfo().offset,
            &id,
            0) == 0)
        {
            pass = true;
            hasModifier = false;
            goto end;
        }
    }

    if (!bo->hasModifier() && bo->planeCount() == 1)
    {
        const RFormatInfo *info { RDRMFormat::GetInfo(format) };

        if (info)
        {
            if (drmModeAddFB(
                device->drmFd(),
                bo->dmaInfo().width,
                bo->dmaInfo().height,
                info->depth,
                info->bpp,
                bo->dmaInfo().stride[0],
                handles[0],
                &id) == 0)
            {
                pass = true;
                hasModifier = false;
                goto end;
            }
        }

        format = RDRMFormat::GetAlphaSubstitute(format);

        if (format != bo->dmaInfo().format)
        {
            info = RDRMFormat::GetInfo(format);

            if (info)
            {
                if (drmModeAddFB(
                    device->drmFd(),
                    bo->dmaInfo().width,
                    bo->dmaInfo().height,
                    info->depth,
                    info->bpp,
                    bo->dmaInfo().stride[0],
                    handles[0],
                    &id) == 0)
                {
                    pass = true;
                    hasModifier = false;
                    goto end;
                }

                id = 0;
            }
        }
    }
end:

    if (!pass)
    {
        device->log(CZError, CZLN, "Failed to create RDRMFramebuffer from RGBMBo");
        return {};
    }

    std::vector<UInt32> handlesVec;
    for (int i = 0; i < bo->planeCount(); i++)
        handlesVec.emplace_back(handles[i]);

    return std::shared_ptr<RDRMFramebuffer>(new RDRMFramebuffer(
        core, device, id,
        { bo->dmaInfo().width, bo->dmaInfo().height },
        format,
        bo->dmaInfo().modifier,
        hasModifier, handlesVec, CZOwnership::Borrow));
}

std::shared_ptr<RDRMFramebuffer> RDRMFramebuffer::MakeFromDMA(const RDMABufferInfo &dmaInfo, RDevice *importer) noexcept
{
    auto core { RCore::Get() };

    if (!core)
    {
        RLog(CZError, CZLN, "Missing RCore");
        return {};
    }

    if (!importer)
        importer = core->mainDevice();


    UInt32 id { 0 };
    bool hasModifier;
    bool pass { false };
    UInt32 handles[4] { 0, 0, 0, 0 };
    RModifier modifiers[4] {};
    RFormat format { dmaInfo.format };
    RDevice *device { importer };

    for (int i = 0; i < dmaInfo.planeCount; i++)
    {
        if (drmPrimeFDToHandle(device->drmFd(), dmaInfo.fd[i], &handles[i]) != 0)
        {
            device->log(CZError, CZLN, "drmPrimeFDToHandle failed");
            closeHandles(device->drmFd(), handles, i+1);
            return {};
        }

        modifiers[i] = dmaInfo.modifier;
    }

    if (device->caps().AddFb2Modifiers && dmaInfo.modifier != DRM_FORMAT_MOD_INVALID)
    {
        if (drmModeAddFB2WithModifiers(
                device->drmFd(),
                dmaInfo.width,
                dmaInfo.height,
                format,
                handles,
                dmaInfo.stride,
                dmaInfo.offset,
                modifiers,
                &id,
                DRM_MODE_FB_MODIFIERS) == 0)
        {
            pass = true;
            hasModifier = true;
            goto end;
        }
    }

    if (dmaInfo.modifier == DRM_FORMAT_MOD_INVALID || dmaInfo.modifier == DRM_FORMAT_MOD_LINEAR)
    {
        if (drmModeAddFB2(
                device->drmFd(),
                dmaInfo.width,
                dmaInfo.height,
                format,
                handles,
                dmaInfo.stride,
                dmaInfo.offset,
                &id,
                0) == 0)
        {
            pass = true;
            hasModifier = false;
            goto end;
        }

        if (dmaInfo.planeCount == 1)
        {
            const RFormatInfo *info { RDRMFormat::GetInfo(format) };

            if (info)
            {
                if (drmModeAddFB(
                        device->drmFd(),
                        dmaInfo.width,
                        dmaInfo.height,
                        info->depth,
                        info->bpp,
                        dmaInfo.stride[0],
                        handles[0],
                        &id) == 0)
                {
                    pass = true;
                    hasModifier = false;
                    goto end;
                }
            }

            format = RDRMFormat::GetAlphaSubstitute(format);

            if (format != dmaInfo.format)
            {
                info = RDRMFormat::GetInfo(format);

                if (info)
                {
                    if (drmModeAddFB(
                            device->drmFd(),
                            dmaInfo.width,
                            dmaInfo.height,
                            info->depth,
                            info->bpp,
                            dmaInfo.stride[0],
                            handles[0],
                            &id) == 0)
                    {
                        pass = true;
                        hasModifier = false;
                        goto end;
                    }

                    id = 0;
                }
            }
        }
    }

end:

    if (!pass)
    {
        device->log(CZError, CZLN, "Failed to create RDRMFramebuffer from RGBMBo");
        closeHandles(device->drmFd(), handles, dmaInfo.planeCount);
        return {};
    }

    std::vector<UInt32> handlesVec;
    for (int i = 0; i < dmaInfo.planeCount; i++)
        handlesVec.emplace_back(handles[i]);

    return std::shared_ptr<RDRMFramebuffer>(new RDRMFramebuffer(
        core, device, id,
        { dmaInfo.width, dmaInfo.height },
        format,
        dmaInfo.modifier, hasModifier,
        handlesVec, CZOwnership::Own));
}

RDRMFramebuffer::RDRMFramebuffer(std::shared_ptr<RCore> core,
                                 RDevice *device, UInt32 id,
                                 SkISize size, RFormat format,
                                 RModifier modifier, bool hasModifier,
                                 const std::vector<UInt32> &handles,
                                 CZOwnership handlesOwnership) noexcept :
    m_id(id),
    m_size(size),
    m_device(device),
    m_format(format),
    m_modifier(modifier),
    m_core(core),
    m_handles(handles),
    m_handlesOwn(handlesOwnership),
    m_hasModifier(hasModifier)
{
}

RDRMFramebuffer::~RDRMFramebuffer() noexcept
{
    drmModeRmFB(m_device->drmFd(), m_id);

    if (m_handlesOwn == CZOwnership::Own)
        closeHandles(m_device->drmFd(), m_handles.data(), m_handles.size());
}
