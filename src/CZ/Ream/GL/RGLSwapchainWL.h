#ifndef RGLSWAPCHAINWL_H
#define RGLSWAPCHAINWL_H

#include <CZ/Ream/WL/RWLSwapchain.h>
#include <wayland-egl-core.h>
#include <EGL/egl.h>

class CZ::RGLSwapchainWL : public RWLSwapchain
{
public:
    ~RGLSwapchainWL() noexcept;
    std::optional<const RSwapchainImage> acquire() noexcept override;
    bool present(const RSwapchainImage &image, SkRegion *damage = nullptr) noexcept override;
    bool resize(SkISize size) noexcept override;
private:
    friend class RWLSwapchain;
    static std::shared_ptr<RGLSwapchainWL> Make(wl_surface *surface, SkISize size) noexcept;
    RGLSwapchainWL(std::shared_ptr<RGLCore> core, RGLDevice *device, std::shared_ptr<RGLImage> image, wl_egl_window *window, wl_surface *surface, EGLSurface eglSurface, SkISize size) noexcept;
    std::shared_ptr<RGLCore> m_core;
    RGLDevice *m_device;
    std::shared_ptr<RGLImage> m_image;
    EGLSurface m_eglSurface { EGL_NO_SURFACE };
    wl_egl_window *m_window;
    bool m_acquired { false };
};

#endif // RGLSWAPCHAINWL_H
