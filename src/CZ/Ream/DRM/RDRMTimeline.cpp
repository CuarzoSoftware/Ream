#include <CZ/Ream/RDevice.h>
#include <CZ/Ream/RCore.h>
#include <CZ/Ream/DRM/RDRMTimeline.h>
#include <CZ/Ream/RLog.h>
#include <CZ/Core/CZEventSource.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <sys/eventfd.h>

using namespace CZ;

std::shared_ptr<RDRMTimeline> RDRMTimeline::Make(RDevice *device) noexcept
{
    auto core { RCore::Get() };

    if (!core)
    {
        RLog(CZError, CZLN, "Missing RCore");
        return {};
    }

    if (!device)
        device = core->mainDevice();

    // TODO: Check timeline cap

    UInt32 handle;
    if (drmSyncobjCreate(device->drmFd(), 0, &handle) != 0)
    {
        device->log(CZError, CZLN, "drmSyncobjCreate failed");
        return {};
    }

    return std::shared_ptr<RDRMTimeline>(new RDRMTimeline(core, device, handle));
}

std::shared_ptr<RDRMTimeline> RDRMTimeline::Import(int timelineFd, CZOwn own, RDevice *device) noexcept
{
    auto core { RCore::Get() };

    if (!core)
    {
        RLog(CZError, CZLN, "Missing RCore");
        return {};
    }

    if (!device)
        device = core->mainDevice();

    // TODO: Check timeline cap

    UInt32 handle {};

    if (drmSyncobjFDToHandle(device->drmFd(), timelineFd, &handle) != 0)
    {
        device->log(CZError, CZLN, "drmSyncobjFDToHandle failed");

        if (own == CZOwn::Own)
            close(timelineFd);
        return {};
    }

    if (own == CZOwn::Own)
        close(timelineFd);
    return std::shared_ptr<RDRMTimeline>(new RDRMTimeline(core, device, handle));
}

int RDRMTimeline::exportTimeline() const noexcept
{
    int fd { -1 };

    if (drmSyncobjHandleToFD(device()->drmFd(), handle(), &fd) != 0)
    {
        RLog(CZError, CZLN, "drmSyncobjHandleToFD failed");
        return -1;
    }

    return fd;
}

int RDRMTimeline::exportSyncFile(UInt64 point) const noexcept
{
    int fd { -1 };
    UInt32 tmpHandle;

    if (drmSyncobjCreate(device()->drmFd(), 0, &tmpHandle) != 0)
    {
        RLog(CZError, CZLN, "drmSyncobjCreate failed");
        return -1;
    }

    if (drmSyncobjTransfer(device()->drmFd(), tmpHandle, 0, handle(), point, 0) != 0)
    {
        RLog(CZError, CZLN, "drmSyncobjTransfer failed");
        drmSyncobjDestroy(device()->drmFd(), tmpHandle);
        return -1;
    }

    if (drmSyncobjExportSyncFile(device()->drmFd(), tmpHandle, &fd) != 0)
    {
        RLog(CZError, CZLN, "drmSyncobjExportSyncFile failed");
        drmSyncobjDestroy(device()->drmFd(), tmpHandle);
        return -1;
    }

    drmSyncobjDestroy(device()->drmFd(), tmpHandle);
    return fd;
}

bool RDRMTimeline::importSyncFile(int fd, UInt64 point, CZOwn own) noexcept
{
    bool ret { false };

    UInt32 tmpHandle;

    if (drmSyncobjCreate(device()->drmFd(), 0, &tmpHandle) != 0)
    {
        RLog(CZError, CZLN, "drmSyncobjCreate failed");
        goto closeFd;
    }

    if (drmSyncobjImportSyncFile(device()->drmFd(), tmpHandle, fd) != 0)
    {
        RLog(CZError, CZLN, "drmSyncobjImportSyncFile failed");
        goto destroySyncobj;
    }

    if (drmSyncobjTransfer(device()->drmFd(), handle(), point, tmpHandle, 0, 0) != 0)
    {
        RLog(CZError, CZLN, "drmSyncobjTransfer failed");
        goto destroySyncobj;
    }

    ret = true;

destroySyncobj:
    drmSyncobjDestroy(device()->drmFd(), tmpHandle);

closeFd:
    if (own == CZOwn::Own)
        close(fd);

    return ret;
}

bool RDRMTimeline::transferPoint(UInt64 srcPoint, std::shared_ptr<RDRMTimeline> dst, UInt64 dstPoint) noexcept
{
    if (!dst)
    {
        RLog(CZError, CZLN, "Invalid dst timeline (nullptr)");
        return false;
    }

    if (device() != dst->device())
    {
        RLog(CZError, CZLN, "Invalid dst timeline (devices differ)");
        return false;
    }

    if (drmSyncobjTransfer(dst->device()->drmFd(), dst->handle(), dstPoint, handle(), srcPoint, 0) != 0)
    {
        RLog(CZError, CZLN, "drmSyncobjTransfer failed");
        return false;
    }

    return true;
}

int RDRMTimeline::waitSync(UInt64 point, UInt32 flags, Int64 timeoutNs) noexcept
{
    UInt32 first;
    const auto ret { drmSyncobjTimelineWait(device()->drmFd(), &m_handle, &point, 1, timeoutNs, flags, &first) };

    if (ret == 0)
        return 1;
    else if (ret == -ETIME)
        return 0;
    else
        return -1;
}

std::shared_ptr<CZEventSource> RDRMTimeline::waitAsync(UInt64 point, UInt32 flags, Callback callback) noexcept
{
    if (!callback)
    {
        RLog(CZError, CZLN, "Missing callback");
        return {};
    }

    const int fd { eventfd(0, EFD_CLOEXEC) };

    if (fd < 0)
    {
        RLog(CZError, CZLN, "Failed to create eventfd");
        return {};
    }

    struct drm_syncobj_eventfd sje {};
    sje.handle = handle();
    sje.flags = flags;
    sje.fd = fd;
    sje.point = point;

    if (drmIoctl(device()->drmFd(), DRM_IOCTL_SYNCOBJ_EVENTFD, &sje) != 0)
    {
        RLog(CZError, CZLN, "Failed to link eventfd");
        close(fd);
        return {};
    }

    auto source { CZEventSource::Make(fd, EPOLLIN, CZOwn::Own, [this, callback](int, UInt32) {
        callback(this);
    }) };

    if (!source)
    {
        RLog(CZError, CZLN, "Failed to create event source");
        return {};
    }

    return source;
}

bool RDRMTimeline::signalPoint(UInt64 point) noexcept
{
    if (drmSyncobjTimelineSignal(device()->drmFd(), &m_handle, &point, 1) != 0)
    {
        RLog(CZError, CZLN, "drmSyncobjTimelineSignal failed");
        return false;
    }
    return true;
}

RDRMTimeline::~RDRMTimeline() noexcept
{
    drmSyncobjDestroy(m_device->drmFd(), m_handle);
}

RDRMTimeline::RDRMTimeline(std::shared_ptr<RCore> core, RDevice *device, UInt32 handle) noexcept :
    m_core(core),
    m_device(device),
    m_handle(handle)
{}
