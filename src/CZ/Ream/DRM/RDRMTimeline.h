#ifndef RDRMTIMELINE_H
#define RDRMTIMELINE_H

#include <CZ/Ream/RObject.h>
#include <CZ/Core/CZOwn.h>
#include <CZ/Core/CZSpFd.h>
#include <memory>

class CZ::RDRMTimeline : public RObject
{
public:
    using Callback = std::function<void(RDRMTimeline *timeline)>;

    static std::shared_ptr<RDRMTimeline> Make(RDevice *device = nullptr) noexcept;
    static std::shared_ptr<RDRMTimeline> Import(int timelineFd, CZOwn own, RDevice *device = nullptr) noexcept;

    // -1 on failure
    [[nodiscard]] CZSpFd exportTimeline() const noexcept;

    // -1 on failure
    [[nodiscard]] CZSpFd exportSyncFile(UInt64 point) const noexcept;
    bool importSyncFile(int fd, UInt64 point, CZOwn own) noexcept;

    // dst must belong to the same device
    bool transferPoint(UInt64 srcPoint, std::shared_ptr<RDRMTimeline> dst, UInt64 dstPoint) noexcept;

    // -1 error, 0 timeout, 1 ok
    int waitSync(UInt64 point, UInt32 flags, Int64 timeoutNs = 0) noexcept;

    // @flags: Zero to wait for the point to be signalled, or DRM_SYNCOBJ_WAIT_FLAGS_WAIT_AVAILABLE
    // to wait for a fence to be available for the point.
    // Returns nullptr on failure
    [[nodiscard]] std::shared_ptr<CZEventSource> waitAsync(UInt64 point, UInt32 flags, Callback callback) noexcept;

    bool signalPoint(UInt64 point) noexcept;

    std::shared_ptr<RCore> core() const noexcept { return m_core; }
    RDevice *device() const noexcept { return m_device; }
    UInt32 handle() const noexcept { return m_handle; }

    ~RDRMTimeline() noexcept;
private:
    RDRMTimeline(std::shared_ptr<RCore> core, RDevice *device, UInt32 handle) noexcept;
    std::shared_ptr<RCore> m_core;
    RDevice *m_device;
    UInt32 m_handle;
};

#endif // RDRMTIMELINE_H
