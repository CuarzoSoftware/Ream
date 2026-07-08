#ifndef CZ_RGLSWAPCHAINWL_H
#define CZ_RGLSWAPCHAINWL_H

#include <CZ/Ream/WL/RWLSwapchain.h>
#include <wayland-egl-core.h>
#include <EGL/egl.h>

/**
 * @brief OpenGL backend implementation of RWLSwapchain.
 *
 * Presents to a Wayland surface via EGL. It creates a `wl_egl_window` and an EGLSurface for the
 * given `wl_surface`, wraps that surface as an RGLImage, and drives presentation with
 * `eglSwapBuffers` (using `eglSwapBuffersWithDamageKHR` when damage and the extension are
 * available). Created via RWLSwapchain::Make().
 */
class CZ::RGLSwapchainWL : public RWLSwapchain
{
public:
    /**
     * @brief Destroys the EGL surface and the `wl_egl_window`.
     */
    ~RGLSwapchainWL() noexcept;

    /**
     * @brief Acquires the swapchain image to render the next frame into.
     *
     * @return The swapchain image (with its buffer age), or `std::nullopt` if an image is already
     *         acquired and not yet presented.
     */
    std::optional<const RSwapchainImage> acquire() noexcept override;

    /**
     * @brief Presents a previously acquired image to the Wayland surface.
     *
     * @param image  The image returned by acquire().
     * @param damage Optional damage region (in image coordinates); enables damage-aware swapping when
     *               supported, otherwise the whole surface is swapped.
     * @return `true` on success, `false` if the image does not match or was not acquired.
     */
    bool present(const RSwapchainImage &image, SkRegion *damage = nullptr) noexcept override;

    /**
     * @brief Resizes the swapchain (and the underlying `wl_egl_window`).
     *
     * @param size The new size in pixels. Must be non-empty.
     * @return `true` on success (including when the size is unchanged), `false` if the size is invalid.
     */
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

#endif // CZ_RGLSWAPCHAINWL_H
