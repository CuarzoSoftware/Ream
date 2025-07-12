#include <CZ/Ream/DRM/RDRMPlatformHandle.h>
#include <CZ/Ream/RLog.h>
#include <fcntl.h>

using namespace CZ;

std::shared_ptr<RDRMPlatformHandle> RDRMPlatformHandle::Make(const std::unordered_set<RDRMFdHandle> &fds) noexcept
{
    if (fds.empty())
    {
        RError(CZLN, "The set is empty.");
        return nullptr;
    }

    std::unordered_set<RDRMFdHandle> dupSet;

    for (const RDRMFdHandle &handle : fds)
    {
        RDRMFdHandle dupHandle { handle };
        dupHandle.fd = fcntl(handle.fd, F_DUPFD_CLOEXEC, 0);

        if (dupHandle.fd < 0)
        {
            RError(CZLN, "Failed to duplicate DRM fd.");
            continue;
        }

        dupSet.emplace(dupHandle);
    }

    if (dupSet.empty())
    {
        RError(CZLN, "Failed to duplicate at least one DRM fd.");
        return nullptr;
    }

    return std::shared_ptr<RDRMPlatformHandle>(new RDRMPlatformHandle(dupSet));
}

RDRMPlatformHandle::~RDRMPlatformHandle() noexcept
{
    while (!m_fds.empty())
    {
        close(m_fds.begin()->fd);
        m_fds.erase(m_fds.begin());
    }
}
