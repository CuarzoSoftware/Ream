#include "CZ/Ream/DRM/RDRMFormat.h"
#include <CZ/Ream/DRM/RDRMFramebuffer.h>
#include <CZ/Ream/RLog.h>
#include <CZ/Ream/GBM/RGBMBo.h>
#include <CZ/Ream/RCore.h>
#include <CZ/Ream/RDevice.h>
#include <cstring>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm_fourcc.h>

using namespace CZ;

std::shared_ptr<RDRMFramebuffer> RDRMFramebuffer::MakeFromGBMBo(std::shared_ptr<RGBMBo> bo) noexcept
{
    auto core { RCore::Get() };

    if (!core)
    {
        RLog(CZError, CZLN, "Missing RCore");
        return {};
    }

    if (!bo)
    {
        RLog(CZError, CZLN, "Invalid RGBMBo");
        return {};
    }

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

    if (device->caps().AddFb2Modifiers && (bo->hasModifier() || bo->modifier() == DRM_FORMAT_MOD_LINEAR)) // TODO: Check addFb2 cap
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

    return std::shared_ptr<RDRMFramebuffer>(new RDRMFramebuffer(core, device, id, bo->dmaInfo().width, bo->dmaInfo().height, hasModifier, handlesVec, CZOwnership::Borrow));
}

RDRMFramebuffer::RDRMFramebuffer(std::shared_ptr<RCore> core, RDevice *device, UInt32 id, Int32 width, Int32 height, bool hasModifier, const std::vector<UInt32> &handles, CZOwnership handlesOwnership) noexcept :
    m_id(id),
    m_width(width),
    m_height(height),
    m_device(device),
    m_core(core),
    m_handles(handles),
    m_handlesOwn(handlesOwnership),
    m_hasModifier(hasModifier)
{
}

RDRMFramebuffer::~RDRMFramebuffer() noexcept
{
    if (m_handlesOwn == CZOwnership::Own)
        for (UInt32 handle : m_handles)
            drmCloseBufferHandle(m_device->drmFd(), handle);

    drmModeRmFB(m_device->drmFd(), m_id);
}
