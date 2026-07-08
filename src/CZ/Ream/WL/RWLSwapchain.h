#ifndef RWLSWAPCHAIN_H
#define RWLSWAPCHAIN_H

#include <CZ/Ream/RSwapchain.h>

struct wl_surface;

/**
 * @brief Abstract swapchain that presents to a Wayland client surface.
 *
 * RWLSwapchain specializes RSwapchain for the Wayland platform, associating the swapchain with a
 * client @c wl_surface to which acquired images are presented. The concrete implementation is
 * chosen by the active graphics API (Raster, OpenGL or Vulkan) at construction time.
 */
class CZ::RWLSwapchain : public RSwapchain
{
public:
    /**
     * @brief Creates a Wayland swapchain for the given surface.
     *
     * Dispatches to the concrete backend matching the active graphics API.
     *
     * @param surface The target @c wl_surface. Must not be null.
     * @param size    Initial image size in pixels. Must not be empty.
     *
     * @return A valid swapchain on success, nullptr on failure.
     */
    static std::shared_ptr<RWLSwapchain> Make(wl_surface *surface, SkISize size) noexcept;

    /**
     * @brief Returns the Wayland surface this swapchain presents to.
     */
    wl_surface *surface() const noexcept { return m_surface; }
protected:
    RWLSwapchain(SkISize size, wl_surface *surface) noexcept :
        RSwapchain(size),
        m_surface(surface) {}
    wl_surface *m_surface;
};

#endif // RWLSWAPCHAIN_H
