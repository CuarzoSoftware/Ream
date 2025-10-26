#include <CZ/Ream/DRM/RDRMFormat.h>
#include <CZ/Ream/DRM/RDRMFramebuffer.h>
#include <CZ/Ream/GBM/RGBMBo.h>
#include <CZ/Ream/RResourceTracker.h>
#include <CZ/Ream/RLog.h>
#include <CZ/Ream/RCore.h>
#include <CZ/Ream/RDevice.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm_fourcc.h>
#include <unordered_set>

using namespace CZ;

static void CloseHandles(int drmFd, UInt32 *handles, size_t n) noexcept
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

static bool CreateFb(RDevice *dev, int n, UInt32 w, UInt32 h, RFormat fmt, const RModifier *mods, const UInt32 *handles, const UInt32 *strides, const UInt32 *offsets, UInt32 *outId, bool *outHasMod) noexcept
{
    if (dev->caps().AddFb2Modifiers)
    {
        UInt32 flags { 0 };

        if (mods[0] && mods[0] != DRM_FORMAT_MOD_INVALID)
            flags = DRM_MODE_FB_MODIFIERS;

        if (drmModeAddFB2WithModifiers(dev->drmFd(), w, h, fmt, handles, strides, offsets, mods, outId, flags) == 0)
        {
            *outHasMod = flags != 0;
            return true;
        }
    }

    if (mods[0] == DRM_FORMAT_MOD_INVALID || mods[0] == DRM_FORMAT_MOD_LINEAR)
    {
        if (drmModeAddFB2(dev->drmFd(), w, h, fmt, handles, strides, offsets, outId, 0) == 0)
        {
            *outHasMod = false;
            return true;
        }
    }

    if ((mods[0] == DRM_FORMAT_MOD_INVALID || mods[0] == DRM_FORMAT_MOD_LINEAR) && n == 1)
    {
        const RFormatInfo *info { RDRMFormat::GetInfo(fmt) };

        if (info)
        {
            if (drmModeAddFB(dev->drmFd(), w, h, info->depth, info->bpp, strides[0], handles[0], outId) == 0)
            {
                *outHasMod = false;
                return true;
            }
        }

        const RFormat prev { fmt };
        fmt = RDRMFormat::GetAlphaSubstitute(fmt);

        if (fmt != prev)
        {
            info = RDRMFormat::GetInfo(fmt);

            if (info)
            {
                if (drmModeAddFB(dev->drmFd(), w, h, info->depth, info->bpp, strides[0], handles[0], outId) == 0)
                {
                    *outHasMod = false;
                    return true;
                }
            }
        }
    }

    return false;
}

std::shared_ptr<RDRMFramebuffer> RDRMFramebuffer::MakeFromGBMBo(std::shared_ptr<RGBMBo> bo) noexcept
{
    if (!bo)
        return {};

    auto core { bo->core() };
    UInt32 id { 0 };
    bool hasModifier;
    UInt32 handles[4] { 0, 0, 0, 0 };
    UInt32 stride[4] { 0, 0, 0, 0 };
    UInt32 offset[4] { 0, 0, 0, 0 };
    RModifier modifiers[4] { 0, 0, 0, 0 };
    RFormat format { bo->format() };
    RDevice *device { bo->allocator() };

    for (int i = 0; i < bo->planeCount(); i++)
    {
        handles[i] = bo->planeHandle(i).u32;
        stride[i] = bo->planeStride(i);
        offset[i] = bo->planeOffset(i);
        modifiers[i] = bo->modifier();
    }

    auto size { bo->size() };

    if (!CreateFb(bo->allocator(), bo->planeCount(), size.width(), size.height(), format, modifiers, handles, stride, offset, &id, &hasModifier))
    {
        device->log(CZError, CZLN, "Failed to create RDRMFramebuffer from RGBMBo");
        return {};
    }

    std::vector<UInt32> handlesVec;
    for (int i = 0; i < bo->planeCount(); i++)
        handlesVec.emplace_back(handles[i]);

    return std::shared_ptr<RDRMFramebuffer>(new RDRMFramebuffer(
        core, device, id,
        size,
        format,
        modifiers[0],
        hasModifier, handlesVec, CZOwn::Borrow));
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
    UInt32 handles[4] { 0, 0, 0, 0 };
    RModifier modifiers[4] {};
    RFormat format { dmaInfo.format };
    RDevice *device { importer };

    for (int i = 0; i < dmaInfo.planeCount; i++)
    {
        if (drmPrimeFDToHandle(device->drmFd(), dmaInfo.fd[i], &handles[i]) != 0)
        {
            device->log(CZError, CZLN, "drmPrimeFDToHandle failed");
            CloseHandles(device->drmFd(), handles, i+1);
            return {};
        }

        modifiers[i] = dmaInfo.modifier;
    }

    if (!CreateFb(device, dmaInfo.planeCount, dmaInfo.width, dmaInfo.height, format, modifiers, handles, dmaInfo.stride, dmaInfo.offset, &id, &hasModifier))
    {
        device->log(CZError, CZLN, "Failed to create RDRMFramebuffer from RGBMBo");
        CloseHandles(device->drmFd(), handles, dmaInfo.planeCount);
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
        handlesVec, CZOwn::Own));
}

std::shared_ptr<RDRMFramebuffer> RDRMFramebuffer::WrapHandle(SkISize size, UInt32 stride, RFormat format, RModifier modifier, UInt32 handle, CZOwn ownership, RDevice *device) noexcept
{
    auto core { RCore::Get() };

    if (!core)
    {
        RLog(CZError, CZLN, "Missing RCore");
        return {};
    }

    if (!device)
        device = core->mainDevice();

    UInt32 handles[4] { handle, 0, 0, 0 };
    const RModifier mods[4] { modifier, 0, 0, 0 };
    const UInt32 strides[4] { stride, 0, 0, 0 };
    const UInt32 offsets[4] { 0, 0, 0, 0 };

    UInt32 id;
    bool hasModifier;

    if (!CreateFb(device, 1, size.width(), size.height(), format, mods, handles, strides, offsets, &id, &hasModifier))
    {
        device->log(CZError, CZLN, "Failed to create RDRMFramebuffer from handles");
        if (ownership == CZOwn::Own)
            CloseHandles(device->drmFd(), handles, 1);
        return {};
    }

    std::vector<UInt32> handlesVec;
    handlesVec.emplace_back(handle);

    return std::shared_ptr<RDRMFramebuffer>(new RDRMFramebuffer(
        core, device, id,
        size, format,
        modifier, hasModifier,
        handlesVec, ownership));
}

RDRMFramebuffer::RDRMFramebuffer(std::shared_ptr<RCore> core,
                                 RDevice *device, UInt32 id,
                                 SkISize size, RFormat format,
                                 RModifier modifier, bool hasModifier,
                                 const std::vector<UInt32> &handles,
                                 CZOwn handlesOwnership) noexcept :
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
    RResourceTrackerAdd(RDRMFramebufferRes);
}

RDRMFramebuffer::~RDRMFramebuffer() noexcept
{
    int ret { drmModeCloseFB(m_device->drmFd(), m_id) };

    if (ret == -EINVAL)
        ret = drmModeRmFB(m_device->drmFd(), m_id);

    if (ret != 0)
        m_device->log(CZError, "Failed to close FB: {}", strerror(-ret));

    if (m_handlesOwn == CZOwn::Own)
        CloseHandles(m_device->drmFd(), m_handles.data(), m_handles.size());

    RResourceTrackerSub(RDRMFramebufferRes);
}
