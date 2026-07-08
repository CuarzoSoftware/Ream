#ifndef CZ_RVKSWAPCHAINWL_H
#define CZ_RVKSWAPCHAINWL_H

#include <CZ/Ream/WL/RWLSwapchain.h>
#include <vulkan/vulkan.h>
#include <memory>
#include <vector>

/**
 * @brief Wayland-client Vulkan swapchain (VK_KHR_wayland_surface + VkSwapchainKHR).
 *
 * Acquire blocks on a fence (the image must be ready before either the painter or Skia renders
 * into it, and it doubles as frame pacing). Present is asynchronous: the PRESENT_SRC transition
 * is submitted without a CPU wait, signalling a per-image semaphore that vkQueuePresentKHR waits
 * on, so the CPU can run ahead into the next frame (one-frame-deep CPU/GPU pipelining).
 */
class CZ::RVKSwapchainWL final : public RWLSwapchain
{
public:
    ~RVKSwapchainWL() noexcept;
    std::optional<const RSwapchainImage> acquire() noexcept override;
    bool present(const RSwapchainImage &image, SkRegion *damage = nullptr) noexcept override;
    bool resize(SkISize size) noexcept override;
private:
    friend class RWLSwapchain;
    static std::shared_ptr<RVKSwapchainWL> Make(wl_surface *surface, SkISize size) noexcept;
    RVKSwapchainWL(std::shared_ptr<RVKCore> core, RVKDevice *device, wl_surface *surface, SkISize size) noexcept;

    bool createSwapchain(SkISize size, VkSwapchainKHR oldSwapchain) noexcept;
    void destroySwapchain() noexcept;

    std::shared_ptr<RVKCore> m_core;
    RVKDevice *m_device { nullptr };

    VkSurfaceKHR m_vkSurface { VK_NULL_HANDLE };
    VkSwapchainKHR m_swapchain { VK_NULL_HANDLE };
    VkFormat m_format { VK_FORMAT_UNDEFINED };
    VkColorSpaceKHR m_colorSpace { VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
    VkExtent2D m_extent {};
    VkFence m_acquireFence { VK_NULL_HANDLE };
    VkCommandPool m_presentPool { VK_NULL_HANDLE }; // for async PRESENT_SRC transitions

    struct Buffer
    {
        std::shared_ptr<RVKImage> image;
        VkSemaphore presentSem { VK_NULL_HANDLE }; // signalled by the transition, waited by present
        UInt32 lastFrame { 0 };
        bool used { false };
    };
    std::vector<Buffer> m_buffers;

    bool m_acquired { false };
    UInt32 m_currentIndex { 0 };
};

#endif // CZ_RVKSWAPCHAINWL_H
