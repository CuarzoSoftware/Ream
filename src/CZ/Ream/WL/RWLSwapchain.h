#ifndef RWLSWAPCHAIN_H
#define RWLSWAPCHAIN_H

#include <CZ/Ream/RSwapchain.h>

struct wl_surface;

class CZ::RWLSwapchain : public RSwapchain
{
public:
    static std::shared_ptr<RWLSwapchain> Make(wl_surface *surface, SkISize size) noexcept;
    wl_surface *surface() const noexcept { return m_surface; }
protected:
    RWLSwapchain(SkISize size, wl_surface *surface) noexcept :
        RSwapchain(size),
        m_surface(surface) {}
    wl_surface *m_surface;
};

#endif // RWLSWAPCHAIN_H
