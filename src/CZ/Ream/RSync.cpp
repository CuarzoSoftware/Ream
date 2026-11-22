#include <CZ/Ream/RLog.h>
#include <CZ/Ream/RCore.h>
#include <CZ/Ream/RDevice.h>
#include <CZ/Ream/GL/RGLSync.h>
#include <CZ/Ream/VK/RVKSync.h>
#include <CZ/Ream/VK/RVKDevice.h>
#include <CZ/Ream/RResourceTracker.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/sync_file.h>
#include <cstring>

using namespace CZ;

std::shared_ptr<RSync> RSync::FromExternal(int fd, RDevice *device) noexcept
{
    CZSpFd fence { fd };
    auto core { RCore::Get() };

    if (!core)
    {
        RLog(CZError, CZLN, "Missing RCore");
        return {};
    }

    if (core->asGL())
        return RGLSync::FromExternal(fence.release(), (RGLDevice*)device);

    if (core->asVK())
    {
        RVKDevice *vk { device ? device->asVK() : core->mainDevice()->asVK() };
        return RVKSync::FromExternal(fence.release(), vk);
    }

    return {};
}

std::shared_ptr<RSync> RSync::Make(RDevice *device) noexcept
{
    auto core { RCore::Get() };

    if (!core)
    {
        RLog(CZError, CZLN, "Missing RCore");
        return {};
    }

    if (core->asGL())
        return RGLSync::Make((RGLDevice*)device);

    if (core->asVK())
    {
        RVKDevice *vk { device ? device->asVK() : core->mainDevice()->asVK() };
        return RVKSync::Make(vk);
    }

    return {};
}

std::shared_ptr<RSync> RSync::Merge(const RSync &a, const RSync &b, RDevice *device) noexcept
{
    const auto fdA { a.fd() };
    const auto fdB { b.fd() };

    if (fdA.get() < 0 || fdB.get() < 0)
        return {};

    sync_merge_data data {};
    std::strncpy(data.name, "RSync::Merge", sizeof(data.name) - 1);
    data.fd2 = fdB.get();

    if (ioctl(fdA.get(), SYNC_IOC_MERGE, &data) < 0)
    {
        RLog(CZError, CZLN, "SYNC_IOC_MERGE failed: {}", strerror(errno));
        return {};
    }

    return FromExternal(data.fence, device);
}

RSync::RSync(std::shared_ptr<RCore> core, RDevice *device, bool isExternal) noexcept :
    m_isExternal(isExternal),
    m_device(device),
    m_core(core)
{
    assert(device);
    assert(core);
    RResourceTrackerAdd(RSyncRes);
}

RSync::~RSync() noexcept
{
    RResourceTrackerSub(RSyncRes);
}


