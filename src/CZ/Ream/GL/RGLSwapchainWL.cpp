#include <CZ/Ream/GL/RGLSwapchainWL.h>
#include <CZ/Ream/GL/RGLImage.h>
#include <CZ/Ream/RLog.h>

using namespace CZ;

std::shared_ptr<RGLSwapchainWL> RGLSwapchainWL::Make(wl_surface *surface, SkISize size) noexcept
{

}

RGLSwapchainWL::~RGLSwapchainWL() noexcept
{

}

std::optional<const RSwapchainImage> RGLSwapchainWL::acquire() noexcept
{
    if (m_acquired)
        return {};

}

bool RGLSwapchainWL::present(const RSwapchainImage &image, SkRegion *damage) noexcept
{
    if (image.image != m_image)
        return false;


    return true;
}

bool RGLSwapchainWL::resize(SkISize size) noexcept
{
    if (size == m_size)
        return true;

    if (size.isEmpty())
    {
        RLog(CZError, CZLN, "Invalid size {}x{}", size.width(), size.height());
        return false;
    }

    m_size = size;
    wl_egl_window_resize(m_window, size.width(), size.height(), 0, 0);
    REGLSurfaceInfo info {};
    info.size = size;
    info.surface = m_eglSurface;
    info.alphaType = kUnpremul_SkAlphaType;
    info.format = DRM_FORMAT_ABGR8888;
    m_image = RGLImage::FromEGLSurface(info, CZOwn::Borrow);
    return true;
}

RGLSwapchainWL::RGLSwapchainWL(std::shared_ptr<RGLCore> core, RGLDevice *device, std::shared_ptr<RGLImage> image, wl_surface *surface, EGLSurface eglSurface, SkISize size) noexcept :
    RWLSwapchain(size, surface), m_core(core), m_device(device), m_image(image), m_eglSurface(eglSurface)
{}
