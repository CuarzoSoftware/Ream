#include <CZ/Ream/GL/RGLSwapchainWL.h>
#include <CZ/Ream/GL/RGLImage.h>
#include <CZ/Ream/GL/RGLDevice.h>
#include <CZ/Ream/GL/RGLMakeCurrent.h>
#include <CZ/Ream/RCore.h>
#include <CZ/Ream/RLog.h>

using namespace CZ;

static const EGLint eglConfigAttribs[]
{
    EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
    EGL_RED_SIZE, 8,
    EGL_GREEN_SIZE, 8,
    EGL_BLUE_SIZE, 8,
    EGL_ALPHA_SIZE, 0,
    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
    EGL_NONE
};

std::shared_ptr<RGLSwapchainWL> RGLSwapchainWL::Make(wl_surface *surface, SkISize size) noexcept
{
    auto core { RCore::Get() };
    auto *device { core->mainDevice()->asGL() };

    EGLint n;
    EGLConfig eglConfig;

    if (!eglChooseConfig(device->eglDisplay(), eglConfigAttribs, &eglConfig, 1, &n) || n != 1)
    {
        device->log(CZError, CZLN, "Failed to get EGL config");
        return {};
    }

    auto window { wl_egl_window_create(surface, size.width(), size.height()) };

    if (!window)
    {
        device->log(CZError, CZLN, "Failed to create wl_egl_window");
        return {};
    }

    auto eglSurface { eglCreateWindowSurface(device->eglDisplay(), eglConfig,(EGLNativeWindowType) window, NULL) };

    if (eglSurface == EGL_NO_SURFACE)
    {
        device->log(CZError, CZLN, "Failed to create EGLSurface from wl_egl_window");
        wl_egl_window_destroy(window);
        return {};
    }

    auto current { RGLMakeCurrent(device->eglDisplay(), eglSurface, eglSurface, device->eglContext()) };

    REGLSurfaceInfo info {};
    info.size = size;
    info.surface = eglSurface;
    info.alphaType = kUnpremul_SkAlphaType;
    info.format = DRM_FORMAT_ABGR8888;

    auto image { RGLImage::FromEGLSurface(info, CZOwn::Borrow, device) };

    if (!image)
    {
        device->log(CZError, CZLN, "Failed to create RImage from EGLSurface");
        eglDestroySurface(device->eglDisplay(), eglSurface);
        wl_egl_window_destroy(window);
        return {};
    }

    return std::shared_ptr<RGLSwapchainWL>(new RGLSwapchainWL(core->asGL(), device, image, window, surface, eglSurface, size));
}

RGLSwapchainWL::~RGLSwapchainWL() noexcept
{
    eglDestroySurface(m_device->eglDisplay(), m_eglSurface);
    wl_egl_window_destroy(m_window);
}

std::optional<const RSwapchainImage> RGLSwapchainWL::acquire() noexcept
{
    if (m_acquired)
        return {};

    m_acquired = true;
    RSwapchainImage ssImage {};
    ssImage.image = m_image;

    auto current { RGLMakeCurrent(m_device->eglDisplay(), m_eglSurface, m_eglSurface, m_device->eglContext()) };

    EGLint age;
    if (eglQuerySurface(m_device->eglDisplay(), m_eglSurface, EGL_BUFFER_AGE_KHR, &age) == EGL_TRUE)
        ssImage.age = age;
    else
        ssImage.age = 0;

    return ssImage;
}

bool RGLSwapchainWL::present(const RSwapchainImage &image, SkRegion *damage) noexcept
{
    if (image.image != m_image || !m_acquired)
        return false;

    // TODO: handle damage
    m_acquired = false;
    auto current { RGLMakeCurrent(m_device->eglDisplay(), m_eglSurface, m_eglSurface, m_device->eglContext()) };
    eglSwapInterval(m_device->eglDisplay(), 0);
    return eglSwapBuffers(m_device->eglDisplay(), m_eglSurface);
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
    m_image = RGLImage::FromEGLSurface(info, CZOwn::Borrow, m_device);
    return true;
}

RGLSwapchainWL::RGLSwapchainWL(std::shared_ptr<RGLCore> core, RGLDevice *device, std::shared_ptr<RGLImage> image, wl_egl_window *window, wl_surface *surface, EGLSurface eglSurface, SkISize size) noexcept :
    RWLSwapchain(size, surface), m_core(core), m_device(device), m_image(image), m_eglSurface(eglSurface), m_window(window)
{}
