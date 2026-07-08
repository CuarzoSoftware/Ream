#ifndef CZ_RVKDEVICE_H
#define CZ_RVKDEVICE_H

#include <CZ/skia/gpu/ganesh/GrDirectContext.h>
#include <CZ/skia/gpu/vk/VulkanExtensions.h>
#include <CZ/skia/gpu/vk/VulkanMemoryAllocator.h>
#include <CZ/Ream/VK/RVKExtensions.h>
#include <CZ/Ream/RDevice.h>
#include <vulkan/vulkan.h>
#include <unordered_map>
#include <functional>
#include <memory>
#include <thread>
#include <mutex>

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
     * @brief Registers a cleanup to run once @p fence is signalled (deferred/GPU-safe destruction).
     *
     * The fence is destroyed after the cleanup runs. Drained by clearGarbage() (called by the
     * compositor each frame) and fully flushed at device destruction.
     */
    void deferDestroy(VkFence fence, std::function<void()> cleanup) const noexcept;

    /**
     * @brief Queues a semaphore for the next queue submission to wait on.
     *
     * Used by RVKSync::gpuWait so that subsequent GPU work on this device waits for the sync.
     * If @p owned is true the semaphore is destroyed once consumed by a submission.
     */
    void queueWait(VkSemaphore semaphore, bool owned) const noexcept;

    /**
     * @brief Empty queue submission that signals @p semaphore (and optional @p fence) after all
     *        prior queue work.
     *
     * Backs RVKSync::Make (captures "all previously submitted work on this device").
     */
    bool submitSignal(VkSemaphore semaphore, VkFence fence) const noexcept;

    // Drains this device's fence-tracked deferred-destruction queue (Phase 6).
    void clearGarbage() noexcept;

    // Lazily-created shared painter pipeline/render-pass cache.
    RVKPipeline *pipelines() noexcept;
private:
    friend class RVKCore;
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

    // Fence-tracked deferred destruction: cleanup runs once its fence signals (drained by
    // clearGarbage()). Lets submissions be async without freeing in-flight resources.
    mutable std::mutex m_garbageMutex;
    mutable std::vector<std::pair<VkFence, std::function<void()>>> m_garbage;

    std::unique_ptr<RVKPipeline> m_pipelines;
    std::mutex m_pipelinesMutex;

    UInt32 m_score {};
};

#endif // CZ_RVKDEVICE_H
