#ifndef RDRMTIMELINE_H
#define RDRMTIMELINE_H

#include <CZ/Ream/RObject.h>
#include <CZ/Core/CZOwn.h>
#include <CZ/Core/CZSpFd.h>
#include <memory>
#include <functional>

/**
 * @brief A DRM synchronization object (syncobj) timeline for explicit synchronization.
 *
 * Wraps a DRM timeline syncobj, which associates fences with monotonically increasing
 * 64-bit timeline points. Timelines are used for explicit GPU/display synchronization: a
 * producer signals a point once its work completes, and consumers wait on that point before
 * accessing the shared buffer.
 *
 * A timeline is bound to a specific DRM device, which must advertise the Timeline capability.
 * Timelines can be exported/imported as file descriptors for sharing across processes, and
 * individual points can be exported to or imported from `sync_file` file descriptors to
 * interoperate with implicit-sync APIs.
 *
 * Use Make() to create a new timeline, or Import() to adopt an exported one.
 */
class CZ::RDRMTimeline : public RObject
{
public:
    /**
     * @brief Callback type invoked by waitAsync() when a point condition is met.
     *
     * @param timeline The timeline that triggered the callback.
     */
    using Callback = std::function<void(RDRMTimeline *timeline)>;

    /**
     * @brief Creates a new empty timeline syncobj.
     *
     * @param device The DRM device to create the timeline on. If nullptr, RCore::mainDevice()
     *               is used. The device must support the Timeline capability.
     * @return A shared pointer to the created timeline, or nullptr on failure.
     */
    static std::shared_ptr<RDRMTimeline> Make(RDevice *device = nullptr) noexcept;

    /**
     * @brief Imports a timeline from an exported syncobj file descriptor.
     *
     * @param timelineFd A file descriptor previously produced by exportTimeline().
     * @param own        CZOwn::Own to close @p timelineFd before returning (in all cases), or
     *                   CZOwn::Borrow to leave it open for the caller to manage.
     * @param device     The DRM device to import onto. If nullptr, RCore::mainDevice() is used.
     *                   The device must support the Timeline capability.
     * @return A shared pointer to the imported timeline, or nullptr on failure.
     */
    static std::shared_ptr<RDRMTimeline> Import(int timelineFd, CZOwn own, RDevice *device = nullptr) noexcept;

    /**
     * @brief Exports the whole timeline syncobj as a file descriptor.
     *
     * The returned descriptor can be shared and re-imported via Import().
     *
     * @return A valid CZSpFd on success, or an empty/invalid CZSpFd on failure.
     */
    [[nodiscard]] CZSpFd exportTimeline() const noexcept;

    /**
     * @brief Exports the fence at a given timeline point as a `sync_file` file descriptor.
     *
     * @param point The timeline point whose fence should be exported.
     * @return A valid CZSpFd wrapping the sync_file on success, or an empty/invalid CZSpFd on failure.
     */
    [[nodiscard]] CZSpFd exportSyncFile(UInt64 point) const noexcept;

    /**
     * @brief Imports a `sync_file` fence into a given timeline point.
     *
     * @param fd    A `sync_file` file descriptor.
     * @param point The timeline point to associate the imported fence with.
     * @param own   CZOwn::Own to close @p fd before returning (in all cases), or CZOwn::Borrow to
     *              leave it open for the caller to manage.
     * @return true on success, false on failure.
     */
    bool importSyncFile(int fd, UInt64 point, CZOwn own) noexcept;

    /**
     * @brief Copies the fence at a source point of this timeline to a point of another timeline.
     *
     * @param srcPoint The source point on this timeline.
     * @param dst      The destination timeline, which must belong to the same device as this one.
     * @param dstPoint The destination point on @p dst.
     * @return true on success, false if @p dst is null, belongs to a different device, or the
     *         transfer fails.
     */
    bool transferPoint(UInt64 srcPoint, std::shared_ptr<RDRMTimeline> dst, UInt64 dstPoint) noexcept;

    /**
     * @brief Synchronously waits for a timeline point.
     *
     * @param point     The timeline point to wait on.
     * @param flags     DRM syncobj wait flags (e.g. DRM_SYNCOBJ_WAIT_FLAGS_WAIT_AVAILABLE to wait
     *                  for a fence to become available rather than signalled).
     * @param timeoutNs Absolute/relative timeout in nanoseconds as accepted by the DRM syncobj
     *                  timeline wait (0 for an immediate poll).
     * @return 1 if the point was satisfied, 0 on timeout, or -1 on error.
     */
    int waitSync(UInt64 point, UInt32 flags, Int64 timeoutNs = 0) noexcept;

    /**
     * @brief Asynchronously waits for a timeline point via an eventfd-backed event source.
     *
     * Registers an eventfd with the DRM syncobj and returns a CZEventSource that invokes
     * @p callback once the point condition is met. Keep the returned event source alive for as
     * long as the wait should remain active.
     *
     * @param point    The timeline point to wait on.
     * @param flags    Zero to wait for the point to be signalled, or
     *                 DRM_SYNCOBJ_WAIT_FLAGS_WAIT_AVAILABLE to wait for a fence to be available
     *                 for the point.
     * @param callback The callback to invoke; must not be null.
     * @return A shared pointer to the event source, or nullptr on failure (including a null callback).
     */
    [[nodiscard]] std::shared_ptr<CZEventSource> waitAsync(UInt64 point, UInt32 flags, Callback callback) noexcept;

    /**
     * @brief Immediately signals a timeline point.
     *
     * @param point The timeline point to signal.
     * @return true on success, false on failure.
     */
    bool signalPoint(UInt64 point) noexcept;

    /**
     * @brief Returns the internal shared reference to the core instance.
     *
     * This ensures the current RCore instance stays alive while the timeline exists.
     */
    std::shared_ptr<RCore> core() const noexcept { return m_core; }

    /**
     * @brief Returns the DRM device this timeline belongs to.
     */
    RDevice *device() const noexcept { return m_device; }

    /**
     * @brief Returns the DRM syncobj handle backing this timeline.
     */
    UInt32 handle() const noexcept { return m_handle; }

    /**
     * @brief Destroys the underlying DRM syncobj.
     */
    ~RDRMTimeline() noexcept;
private:
    RDRMTimeline(std::shared_ptr<RCore> core, RDevice *device, UInt32 handle) noexcept;
    std::shared_ptr<RCore> m_core;
    RDevice *m_device;
    UInt32 m_handle;
};

#endif // RDRMTIMELINE_H
