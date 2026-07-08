#ifndef RDRMPLATFORMHANDLE_H
#define RDRMPLATFORMHANDLE_H

#include <CZ/Ream/RPlatformHandle.h>
#include <memory>
#include <unordered_set>

struct wl_display;

namespace CZ
{
    /**
     * @brief A DRM device file descriptor paired with arbitrary user data.
     *
     * Used by RDRMPlatformHandle to identify the set of DRM devices a client can access.
     * Equality and hashing are based solely on the file descriptor (@ref fd), so @ref userData
     * does not participate in set membership.
     */
    struct RDRMFdHandle
    {
        /**
         * @brief The DRM device file descriptor.
         */
        int fd;

        /**
         * @brief Opaque user-provided data associated with the file descriptor.
         */
        void *userData;

        /**
         * @brief Compares two handles for equality based on their file descriptor.
         *
         * @param other The handle to compare against.
         * @return true if both handles have the same file descriptor.
         */
        bool operator==(const RDRMFdHandle& other) const
        {
            return fd == other.fd;
        }
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

/**
 * @brief Platform handle for the DRM platform.
 *
 * An RPlatformHandle implementation (see RPlatformHandle) whose platform() is RPlatform::DRM.
 * It holds the set of DRM device file descriptors a client is allowed to use.
 *
 * Use Make() to create an instance.
 */
class CZ::RDRMPlatformHandle : public RPlatformHandle
{
public:
    /**
     * @brief Creates a DRM platform handle from a set of DRM device file descriptors.
     *
     * Each file descriptor is duplicated internally (with F_DUPFD_CLOEXEC), so the caller
     * retains ownership of the passed descriptors. Handles that fail to duplicate are skipped.
     *
     * @param fds A non-empty set of DRM file descriptors (with associated user data).
     * @return A shared pointer to the created handle, or nullptr if @p fds is empty or none of
     *         the descriptors could be duplicated.
     */
    static std::shared_ptr<RDRMPlatformHandle> Make(const std::unordered_set<RDRMFdHandle> &fds) noexcept;

    /**
     * @brief Returns the set of internally duplicated DRM file descriptors.
     */
    const std::unordered_set<RDRMFdHandle> &fds() const noexcept { return m_fds; }

    /**
     * @brief Closes all internally duplicated DRM file descriptors.
     */
    ~RDRMPlatformHandle() noexcept;

private:
    RDRMPlatformHandle(const std::unordered_set<RDRMFdHandle> &fds) noexcept :
        RPlatformHandle(RPlatform::DRM),
        m_fds(fds) {}
    std::unordered_set<RDRMFdHandle> m_fds;
};
#endif // RDRMPLATFORMHANDLE_H
