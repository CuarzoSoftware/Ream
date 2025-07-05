#ifndef RDRMPLATFORMHANDLE_H
#define RDRMPLATFORMHANDLE_H

#include <CZ/Ream/RPlatformHandle.h>
#include <memory>
#include <unordered_set>

struct wl_display;

namespace CZ
{
    struct RDRMFdHandle
    {
        int fd;
        void *userData;
    };
}

namespace std {
    template<>
    struct hash<CZ::RDRMFdHandle>
    {
        std::size_t operator()(const CZ::RDRMFdHandle &h) const
        {
            return std::hash<int>{}(h.fd);
        }
    };
}

class CZ::RDRMPlatformHandle : public RPlatformHandle
{
public:
    // FDS: DRM fds
    // Fds are dupplicated internally
    static std::shared_ptr<RDRMPlatformHandle> Make(const std::unordered_set<RDRMFdHandle> &fds) noexcept;
    const std::unordered_set<RDRMFdHandle> &fds() const noexcept { return m_fds; }

    ~RDRMPlatformHandle() noexcept;

private:
    RDRMPlatformHandle(const std::unordered_set<RDRMFdHandle> &fds) noexcept :
        RPlatformHandle(RPlatform::DRM),
        m_fds(fds) {}
    std::unordered_set<RDRMFdHandle> m_fds;
};
#endif // RDRMPLATFORMHANDLE_H
