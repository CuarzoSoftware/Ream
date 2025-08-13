#include <CZ/Ream/RLog.h>
#include <CZ/Ream/RCore.h>
#include <CZ/Ream/RDevice.h>
#include <CZ/Ream/GL/RGLSync.h>
#include <CZ/Ream/RResourceTracker.h>
#include <fcntl.h>

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

    return {};
}

RSync::RSync(std::shared_ptr<RCore> core, RDevice *device, int fd, bool isExternal) noexcept :
    m_isExternal(isExternal),
    m_fd(fd),
    m_device(device),
    m_core(core)
{
    assert(device);
    assert(core);
    RResourceTrackerAdd(RResourceType::RSyncRes);
}

RSync::~RSync() noexcept
{
    RResourceTrackerSub(RResourceType::RSyncRes);
}


