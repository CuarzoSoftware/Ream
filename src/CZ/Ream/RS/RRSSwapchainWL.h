#ifndef CZ_RRSSWAPCHAINWL_H
#define CZ_RRSSWAPCHAINWL_H

#include <CZ/Ream/WL/RWLSwapchain.h>
#include <CZ/Core/CZWeak.h>
#include <unordered_set>
#include <wayland-client-protocol.h>

/**
 * @brief Raster (software) implementation of RWLSwapchain.
 *
 * RRSSwapchainWL presents CPU-rendered frames to a Wayland @c wl_surface using shared-memory
 * (@c wl_shm) buffers. Each swapchain image is an RRSImage whose backing memory is exported to the
 * compositor as a @c wl_buffer through a @c wl_shm_pool.
 *
 * Buffers are recycled based on compositor release events: acquire() reuses the least recently used
 * released buffer (resizing it if needed) or allocates a new one, and surplus buffers are freed
 * once enough frames have elapsed. Only one image may be acquired at a time.
 */
class CZ::RRSSwapchainWL : public RWLSwapchain
{
public:
    /**
     * @brief Destroys the swapchain and its shared-memory buffers.
     */
    ~RRSSwapchainWL() noexcept;

    /**
     * @brief Acquires the next available buffer. Implements RSwapchain::acquire().
     *
     * @return The acquired swapchain image, or nullopt if an image is already acquired or allocation fails.
     */
    std::optional<const RSwapchainImage> acquire() noexcept override;

    /**
     * @brief Attaches, damages and commits the image to the Wayland surface. Implements RSwapchain::present().
     *
     * If @p damage is null (or the surface is too old for buffer damage) the whole surface is
     * marked damaged; otherwise only the given rects are damaged.
     */
    bool present(const RSwapchainImage &image, SkRegion *damage = nullptr) noexcept override;

    /**
     * @brief Sets the size used for future acquired buffers. Implements RSwapchain::resize().
     *
     * @return false if @p size is empty, true otherwise.
     */
    bool resize(SkISize size) noexcept override;
private:
    friend class RWLSwapchain;

    struct Buffer
    {
        static std::shared_ptr<Buffer> Make(RRSSwapchainWL *swapchain, SkISize size, UInt32 age, UInt32 frame, UInt32 index) noexcept;
        static void onRelease(void *data, wl_buffer *buffer) noexcept;
        Buffer(RRSSwapchainWL *swapchain, const RSwapchainImage &ssImage, wl_shm_pool *pool, wl_buffer *buffer) noexcept;
        void resize(SkISize) noexcept;
        ~Buffer() noexcept;
        CZWeak<RRSSwapchainWL> swapchain;
        RSwapchainImage ssImage;
        wl_shm_pool *pool;
        wl_buffer *buffer;
        bool released { false };
    };

    static std::shared_ptr<RRSSwapchainWL> Make(wl_surface *surface, SkISize size) noexcept;
    RRSSwapchainWL(std::shared_ptr<RRSCore> core, RRSDevice *device, wl_surface *surface, SkISize size) noexcept;
    void freeForgottenBuffers() noexcept;
    std::shared_ptr<RRSCore> m_core;
    RRSDevice *m_device;
    std::vector<std::shared_ptr<Buffer>> m_buffers;
    std::unordered_set<UInt32> m_releasedIdx;
    bool m_acquired { false };
};

#endif // CZ_RRSSWAPCHAINWL_H
