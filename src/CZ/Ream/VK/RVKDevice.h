#ifndef CZ_RVKDEVICE_H
#define CZ_RVKDEVICE_H

#include <CZ/skia/gpu/ganesh/GrDirectContext.h>
#include <CZ/skia/gpu/vk/VulkanExtensions.h>
#include <CZ/skia/gpu/vk/VulkanMemoryAllocator.h>
#include <CZ/Ream/VK/RVKExtensions.h>
#include <CZ/Ream/RDevice.h>
#include <CZ/Ream/DRM/RDRMTimeline.h>
#include <vulkan/vulkan.h>
#include <unordered_map>
#include <functional>
#include <memory>
#include <thread>
#include <chrono>
#include <mutex>
#include <atomic>
#include <condition_variable>

/**
 * @brief Vulkan backend implementation of RDevice.
 *
 * Wraps a single VkPhysicalDevice/VkDevice and its graphics queue, and owns the resources shared
 * across the backend: the painter pipeline cache, per-thread Skia (Ganesh) contexts, a queue mutex
 * serializing submissions, and a fence-tracked deferred-destruction queue enabling asynchronous,
 * non-blocking command submission.
 *
 * Obtained by downcasting an RDevice via RDevice::asVK().
 */
class CZ::RVKDevice final : public RDevice
{
public:
    void wait() noexcept override;
    RVKCore &core() const noexcept { return (RVKCore&)m_core; }

    VkPhysicalDevice physicalDevice() const noexcept { return m_physicalDevice; }
    VkDevice device() const noexcept { return m_device; }
    VkQueue graphicsQueue() const noexcept { return m_graphicsQueue; }
    UInt32 graphicsQueueFamily() const noexcept { return m_graphicsQueueFamily; }

    const VkPhysicalDeviceProperties &properties() const noexcept { return m_properties; }
    const VkPhysicalDeviceMemoryProperties &memoryProperties() const noexcept { return m_memoryProperties; }
    const RVKDeviceProcs &procs() const noexcept { return m_procs; }
    const RVKDeviceExtensions &extensions() const noexcept { return m_ext; }

    bool hasExtension(const char *extension) const noexcept;

    /**
     * @brief Returns the calling thread's Skia (Ganesh) Vulkan context.
     *
     * GrDirectContext is not thread-safe, so a separate context is lazily created per thread,
     * all sharing this device's VkDevice/VkQueue and memory allocator.
     */
    sk_sp<GrDirectContext> skContext() const noexcept;

    /**
     * @brief Serializes access to the shared VkQueue.
     *
     * VkQueue submission and vkDeviceWaitIdle require external synchronization.
     */
    std::mutex &queueMutex() const noexcept { return m_queueMutex; }

    /**
     * @brief Selects a memory type index satisfying the given requirements and property flags.
     * @return The memory type index, or UINT32_MAX if none matches.
     */
    UInt32 findMemoryType(UInt32 typeBits, VkMemoryPropertyFlags props) const noexcept;

    /**
     * @brief Records and synchronously submits a one-shot command buffer on the graphics queue.
     *
     * Serialized via queueMutex() and blocks until the GPU finishes (fence wait).
     * Used for pixel uploads/downloads and layout transitions.
     *
     * @return true on success.
     */
    bool immediateSubmit(const std::function<void(VkCommandBuffer)> &record) const noexcept;

    /**
     * @brief Like immediateSubmit() without waiting: @p done runs once the GPU finished it (e.g. to
     *        free a staging buffer), from any thread's clearGarbage().
     */
    bool immediateSubmitAsync(const std::function<void(VkCommandBuffer)> &record, std::function<void()> done) const noexcept;

    /**
     * @brief Submits an already-recorded command buffer, consuming pending waits, and blocks
     *        until it completes. Used by RVKPainter::flush.
     */
    bool submitCommand(VkCommandBuffer cmd) const noexcept;

    /**
     * @brief Submits an already-recorded command buffer signalling @p fence, consuming pending
     *        waits, WITHOUT blocking. The caller tracks completion via @p fence (typically pairing
     *        with deferDestroy() so CPU/GPU can pipeline). Used by RVKPainter::flush.
     *
     * Owned pending-wait semaphores consumed by this submission are appended to @p ownedWaitsOut;
     * the caller must destroy them once @p fence signals (e.g. inside its deferDestroy cleanup).
     *
     * If @p signal is not VK_NULL_HANDLE it is signalled on completion (e.g. a swapchain present
     * semaphore that vkQueuePresentKHR waits on), so present need not block the CPU.
     */
    bool submitCommandAsync(VkCommandBuffer cmd, VkFence fence, std::vector<VkSemaphore> &ownedWaitsOut,
                            VkSemaphore signal = VK_NULL_HANDLE) const noexcept;

    /**
     * @brief Queues a recorded command buffer for the next batched submission.
     *
     * Each vkQueueSubmit is a driver and kernel round trip (validating every buffer object), and a
     * frame records many passes (e.g. the blur chain), so they are sent together, in the order they
     * were queued. Any other queue submission (immediateSubmit(), submitSignal()...), a present, a
     * Skia submit, wait() and exporting or CPU-waiting a sync of this device send the batch first,
     * so the queue order seen by the GPU stays the recording order (see also flushStaleBatch()).
     *
     * @param cleanup Runs on this thread's clearGarbage() once the GPU finished the command buffer.
     * @param signal  Signalled after it (e.g. a swapchain present semaphore): the batch is sent now.
     * @return false if the batch was sent and failed (the cleanup still runs).
     */
    bool enqueueCommand(VkCommandBuffer cmd, std::function<void()> cleanup, VkSemaphore signal = VK_NULL_HANDLE) const noexcept;

    /// @name Skia work of this thread
    /// Flushing Skia when it has nothing to do still costs (it walks its tasks and buffer pools):
    /// RVKPainter flushes it before recording only if Skia was handed something since (a canvas,
    /// an SkSurface or an SkImage of an image: the only ways to make it record work).
    /// @{
    static void MarkSkiaUsed() noexcept;
    static bool TakeSkiaUsed() noexcept; ///< And clears it
    /// @}

    /// @brief Submits the commands queued by enqueueCommand(), if any (this thread's stream too).
    void flushBatch() const noexcept;

    /// @name Command streams (RVKPainter)
    /// Each thread records its consecutive passes into a single command buffer (its stream) instead
    /// of one per pass: each command buffer costs the driver a reset and a submission entry. The
    /// stream is queued (see enqueueCommand()) by its thread before any other submission of that
    /// thread (present, uploads, readbacks, Skia, wait()...) and by RVKCore::clearGarbage(), or by
    /// another thread needing a sync made while it was open (see resolvePoint()).
    /// @{

    /// @brief This thread's stream, begun if needed, to record a pass into until endStream().
    ///        VK_NULL_HANDLE on failure.
    VkCommandBuffer beginStream() const noexcept;

    /// @brief Ends recording a pass into this thread's stream. @p cleanup runs once the GPU is done.
    void endStream(std::function<void()> cleanup) const noexcept;

    /// @brief Queues this thread's stream for the next batched submission (unless recording a pass).
    void sendOwnStream() const noexcept;

    /// A sync made while its thread's stream was open: its timeline value, once the stream is queued
    struct SyncPoint { std::atomic<UInt64> value { 0 }; };

    /**
     * @brief The timeline value of @p point, queueing the stream of @p owner if still open.
     *
     * From another thread, waits for @p owner to end the pass it may be recording (bounded).
     * @return 0 if it couldn't be queued (e.g. @p owner is this thread, recording a pass).
     */
    UInt64 resolvePoint(const std::shared_ptr<SyncPoint> &point, std::thread::id owner) const noexcept;
    /// @}

    /// @brief flushBatch() if its oldest command waits for 32 ms or more (e.g. nothing presented):
    ///        called by RVKCore::clearGarbage() (each frame and main loop iteration).
    void flushStaleBatch() const noexcept;

    /**
     * @brief Registers a cleanup to run once @p fence is signalled (deferred/GPU-safe destruction).
     *
     * The fence is destroyed after the cleanup runs. Drained by clearGarbage() (called by the
     * compositor each frame) and fully flushed at device destruction.
     */
    void deferDestroy(VkFence fence, std::function<void()> cleanup, bool anyThread = false) const noexcept;

    /**
     * @brief Runs @p cleanup once the GPU finished every command submitted so far (a fence after
     *        them), from any thread's clearGarbage(): no CPU stall (unlike wait()).
     *
     * For resources without thread affinity (e.g. images). Runs right away if the fence can't be
     * submitted, after waiting for the device.
     */
    void destroyWhenIdle(std::function<void()> cleanup) noexcept;

    /**
     * @brief Queues a semaphore for the next queue submission to wait on.
     *
     * Used by RVKSync::gpuWait so that subsequent GPU work of this thread waits for the sync: kept
     * with its stream, handed to the submission of its next work (never another thread's).
     * If @p owned is true the semaphore is destroyed once consumed by a submission.
     * @return The waitsSerial() of the submission that will consume it.
     */
    UInt64 queueWait(VkSemaphore semaphore, bool owned) const noexcept;

    /**
     * @brief Identifies the next submission of this thread's work, which takes the waits of its
     *        queueWait() calls (their return value): while unchanged, a wait queued earlier by this
     *        thread still applies to the commands it records now.
     */
    UInt64 waitsSerial() const noexcept;

    /**
     * @brief Empty queue submission that signals @p semaphore (and optional @p fence) after all
     *        prior queue work.
     *
     * Backs RVKSync::Make (captures "all previously submitted work on this device").
     */
    bool submitSignal(VkSemaphore semaphore, VkFence fence) const noexcept;

    /**
     * @brief Empty queue submission that signals timeline @p semaphore to @p value (and optional
     *        @p fence) after all prior queue work.
     *
     * Backs RVKSync's drm_syncobj-timeline path: the timeline semaphore aliases a drm_syncobj
     * (imported via OPAQUE_FD), so signalling it materializes the syncobj timeline point that a
     * sync_file is later exported from. Does not consume pending waits (they belong to the next
     * real submission).
     */
    bool submitSignalTimeline(VkSemaphore semaphore, UInt64 value, VkFence fence) const noexcept;

    // Drains this device's fence-tracked deferred-destruction queue (Phase 6).
    void clearGarbage() noexcept;

    /// @name Recycled per-pass resources (RVKPainter)
    /// Creating them for every pass was a large part of a frame's CPU time. Returned once the GPU
    /// is done with them (from deferDestroy() cleanups), reset, then taken by the next passes.
    /// Command pools only by the thread that created them (external synchronization).
    /// @{
    struct PassCmd { VkCommandPool pool; VkCommandBuffer cmd; };
    struct PassVbo { VkBuffer buffer; VkDeviceMemory memory; void *mapped; VkDeviceSize capacity; };
    bool takePassCmd(PassCmd &out) const noexcept;
    void recyclePassCmd(const PassCmd &cmd) const noexcept;  ///< Resets it
    bool takePassVbo(PassVbo &out) const noexcept;
    void recyclePassVbo(const PassVbo &vbo) const noexcept;
    VkDescriptorPool takeDescPool() const noexcept;           ///< VK_NULL_HANDLE if none
    void recycleDescPool(VkDescriptorPool pool) const noexcept; ///< Resets it
    /// @}

    // Lazily-created shared painter pipeline/render-pass cache.
    RVKPipeline *pipelines() noexcept;
private:
    friend class RVKCore;
    friend class RVKSync;

    // RVKSync::Make(): a timeline (drm_syncobj imported as a Vulkan timeline semaphore) shared by
    // every sync of this device, a point each, instead of a new syncobj, import, fence and submit
    // per sync. Guarded by m_queueMutex (points must be signalled in increasing order).
    bool initSyncTimeline() const noexcept; // The shared timeline, created once
    // @p pending: set instead of @p point if this thread's stream is open (see SyncPoint)
    bool signalSyncTimeline(std::shared_ptr<RDRMTimeline> &timeline, UInt64 &point, std::shared_ptr<SyncPoint> &pending) const noexcept;

    // Fence-tracked deferred destruction: cleanup runs once its fence signals (drained by
    // clearGarbage()). Lets submissions be async without freeing in-flight resources. Each entry
    // records the thread that created the resources; cleanup MUST run on that same thread because
    // Vulkan command pools require external synchronization (NVIDIA crashes on cross-thread
    // destruction that Mesa tolerates). Hence a list per thread (in its Stream), drained by its
    // clearGarbage(), and one for entries any thread can drain (m_anyGarbage).

    // Shared by the entries of a batch, destroyed with the last one
    struct Fence
    {
        Fence(VkDevice d, VkFence f) noexcept;
        ~Fence() { if (fence != VK_NULL_HANDLE) vkDestroyFence(device, fence, nullptr); }
        VkDevice device;
        VkFence fence; // VK_NULL_HANDLE: already done
        UInt64 serial; // Creation order, ~submission order (see GarbageList::blocking)
        bool signaled { false }; // Cached, queried until it signals
    };
    struct Garbage
    {
        std::shared_ptr<Fence> fence;
        std::function<void()> cleanup;
        std::thread::id owner;
    };
    struct GarbageList
    {
        std::mutex mutex; // Never held while taking another lock
        std::vector<Garbage> items;
        // Its oldest fence not signalled yet: fences of a queue signal in submission order, so while
        // it is pending nothing newer can be done either (one status query, no scan)
        std::shared_ptr<Fence> blocking;
    };
    void pushGarbage(Garbage &&garbage) const noexcept;
    void drainGarbage(GarbageList &list) const noexcept;
    mutable GarbageList m_anyGarbage;

    struct Stream
    {
        GarbageList garbage; // Entries owned by its thread
        std::mutex mutex; // Held by its thread while recording a pass, by whoever queues it
        std::thread::id owner;
        PassCmd cmd {}; // Null pool: not open
        std::vector<std::function<void()>> cleanups;
        std::shared_ptr<SyncPoint> point; // Syncs made while open
        bool recording { false }; // Read and written by its thread only
        std::atomic<bool> sendRequested { false };
        std::mutex waitsMutex; // After m_queueMutex
        std::vector<std::pair<VkSemaphore, bool>> waits; // queueWait() calls of its thread
        UInt64 waitsSerial { 0 }; // Incremented when handed over (see waitsSerial())
    };
    void handOverWaitsLocked(Stream &stream) const noexcept; // m_queueMutex held
    Stream *findStream(std::thread::id thread, bool create) const noexcept;
    void sendStreamLocked(Stream &stream) const noexcept; // stream.mutex held, not m_queueMutex
    UInt64 m_uid { 0 }; // Unique among devices, even with a reused address (findStream())
    mutable std::mutex m_streamsMutex;
    mutable std::unordered_map<std::thread::id, std::unique_ptr<Stream>> m_streams;
    mutable std::mutex m_streamSentMutex;
    mutable std::condition_variable m_streamSent;

    // Skia's vkQueueSubmit/vkQueueWaitIdle (see initSkia()): the batch first, under the queue lock
    static VKAPI_ATTR VkResult VKAPI_CALL SkQueueSubmit(VkQueue queue, uint32_t count, const VkSubmitInfo *submits, VkFence fence);
    static VKAPI_ATTR VkResult VKAPI_CALL SkQueueWaitIdle(VkQueue queue);
    static const RVKDevice *DeviceOfQueue(VkQueue queue) noexcept;
    static std::mutex s_skQueuesMutex;
    static std::vector<std::pair<VkQueue, const RVKDevice*>> s_skQueues;

    // enqueueCommand(): sends the batch (with m_queueMutex held), the next sync point included
    bool flushBatchLocked(VkSemaphore signal = VK_NULL_HANDLE) const noexcept;
    struct BatchEntry { std::function<void()> cleanup; std::thread::id owner; };
    mutable std::vector<VkCommandBuffer> m_batchCmds;
    mutable std::vector<BatchEntry> m_batchEntries;
    mutable bool m_batchSignalsSync { false }; // m_syncPoint is signalled by the batch
    mutable std::chrono::steady_clock::time_point m_batchSince; // First command queued (flushStaleBatch())
    mutable std::shared_ptr<RDRMTimeline> m_syncTimeline;
    mutable VkSemaphore m_syncSemaphore { VK_NULL_HANDLE };
    mutable UInt64 m_syncPoint { 0 };
    mutable int m_syncTimelineState { 0 }; // 0: not tried, 1: ready, -1: unsupported (e.g. NVIDIA)
    static RVKDevice *Make(RVKCore &core, VkPhysicalDevice physicalDevice) noexcept;
    RVKDevice(RVKCore &core, VkPhysicalDevice physicalDevice) noexcept;
    ~RVKDevice() noexcept;
    std::shared_ptr<RPainter> makePainter(std::shared_ptr<RSurface> surface) noexcept override;

    bool init() noexcept;
    bool initPropsAndFeatures() noexcept;
    bool initExtensions() noexcept;
    bool initDRM() noexcept;
    bool initQueues() noexcept;
    bool initDevice() noexcept;
    bool initProcs() noexcept;
    bool initSkia() noexcept;
    bool initFormats() noexcept;

    sk_sp<GrDirectContext> makeSkContext() const noexcept;

    VkPhysicalDevice m_physicalDevice;
    VkDevice m_device { VK_NULL_HANDLE };
    VkQueue m_graphicsQueue { VK_NULL_HANDLE };
    UInt32 m_graphicsQueueFamily { UINT32_MAX };
    VkCommandPool m_commandPool { VK_NULL_HANDLE };

    VkPhysicalDeviceProperties m_properties {};
    VkPhysicalDeviceMemoryProperties m_memoryProperties {};
    VkPhysicalDeviceFeatures m_features {};

    std::vector<VkExtensionProperties> m_extensions;
    std::vector<const char *> m_requiredExtensions;
    RVKDeviceExtensions m_ext {};
    RVKDeviceProcs m_procs {};

    // Feature toggles enabled at device creation (support probed via vkGetPhysicalDeviceFeatures2).
    // Kept alive because the same chain is handed to Skia's VulkanBackendContext.
    VkPhysicalDeviceFeatures2 m_features2 {};
    VkPhysicalDeviceTimelineSemaphoreFeatures m_timelineFeatures {};
    VkPhysicalDeviceSynchronization2FeaturesKHR m_sync2Features {};
    VkPhysicalDeviceDynamicRenderingFeaturesKHR m_dynRenderFeatures {};

    // Skia integration (kept alive for the lifetime of the GrDirectContexts).
    skgpu::VulkanExtensions m_skExtensions {};
    skgpu::VulkanGetProc m_getProc {};
    sk_sp<skgpu::VulkanMemoryAllocator> m_allocator;
    mutable sk_sp<GrDirectContext> m_skContext; // main-thread context, created lazily
    std::thread::id m_mainThread;
    mutable std::mutex m_skContextsMutex;
    mutable std::unordered_map<std::thread::id, sk_sp<GrDirectContext>> m_skContexts;

    mutable std::mutex m_queueMutex;
    // Semaphores the next queue submission must wait on (RVKSync::gpuWait). Guarded by m_queueMutex.
    mutable std::vector<std::pair<VkSemaphore, bool>> m_pendingWaits;


    // See takePassCmd()
    mutable std::mutex m_recycleMutex;
    mutable std::unordered_map<std::thread::id, std::vector<PassCmd>> m_freeCmds;
    mutable std::vector<PassVbo> m_freeVbos;
    mutable std::vector<VkDescriptorPool> m_freeDescPools;

    std::unique_ptr<RVKPipeline> m_pipelines;
    std::mutex m_pipelinesMutex;

    UInt32 m_score {};
};

#endif // CZ_RVKDEVICE_H
