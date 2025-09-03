#ifndef CZ_RRSSWAPCHAINWL_H
#define CZ_RRSSWAPCHAINWL_H

#include <CZ/Ream/WL/RWLSwapchain.h>
#include <CZ/Core/CZWeak.h>
#include <unordered_set>
#include <wayland-client-protocol.h>

class CZ::RRSSwapchainWL : public RWLSwapchain
{
public:
    ~RRSSwapchainWL() noexcept;
    std::optional<const RSwapchainImage> acquire() noexcept override;
    bool present(const RSwapchainImage &image, SkRegion *damage = nullptr) noexcept override;
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
