#include <WL/RWLPlatformHandle.h>
#include <RLog.h>
#include <GL/RGLCore.h>
#include <GL/RGLDevice.h>
#include <GL/RGLMakeCurrent.h>
#include <GL/RGLSwapchainWL.h>
#include <RImage.h>
#include <RSurface.h>
#include <RPainter.h>
#include <RPass.h>

#include <GL/RGLImage.h>

#include <wayland-client.h>
#include <wayland-egl.h>
#include "xdg-shell.h"
#include <drm_fourcc.h>

using namespace CZ;

static std::shared_ptr<RImage> testImage;

static struct
{
    wl_display *wlDisplay;
    wl_registry *wlRegistry;
    wl_compositor *wlCompositor { nullptr };
    xdg_wm_base *xdgWmBase { nullptr };
} app;

struct Window
{
    Window() noexcept;
    void update() noexcept;
    ~Window() noexcept
    {
        xdg_toplevel_destroy(xdgToplevel);
        xdg_surface_destroy(xdgSurface);
        swapchain.reset();
        wl_surface_destroy(wlSurface);
    }

    wl_callback *wlCallback { nullptr };
    wl_surface *wlSurface;
    xdg_surface *xdgSurface;
    xdg_toplevel *xdgToplevel;
    std::shared_ptr<RWLSwapchain> swapchain;
    SkISize size { 200, 200 };
    int32_t scale { 1 };
    bool ready { false };
    bool needsNewSurface { true };
};

static wl_surface_listener wlSurfaceLis
    {
        .enter = [](auto, auto, auto){},
        .leave = [](auto, auto, auto){},
        .preferred_buffer_scale = [](void *data, wl_surface */*wlSurface*/, int32_t factor)
        {
            Window &window { *static_cast<Window*>(data) };

            if (factor == window.scale)
                return;

            window.scale = factor;
            window.needsNewSurface = true;
        },
        .preferred_buffer_transform = [](auto, auto, auto){}
    };

static xdg_surface_listener xdgSurfaceLis
{
    .configure = [](void *data, xdg_surface *xdgSurface, uint32_t serial)
    {
        Window &window { *static_cast<Window*>(data) };
        xdg_surface_ack_configure(xdgSurface, serial);
        window.ready = true;
        window.update();
    }
};

static xdg_toplevel_listener xdgToplevelLis
{
    .configure = [](void *data, xdg_toplevel */*xdgToplevel*/, int32_t width, int32_t height, wl_array */*states*/)
    {
        Window &window { *static_cast<Window*>(data) };
        if (width < 256) width = 256;
        if (height < 256) height = 256;

        width = height = 512;
        if (window.size.width() != width || window.size.height() != height)
        {
            window.size = { width, height };
            window.needsNewSurface = true;
        }
    },
        .close = [](auto, auto){ exit(0); },
        .configure_bounds = [](auto, auto, auto, auto){},
        .wm_capabilities = [](auto, auto, auto){}
};

Window::Window() noexcept
{
    wlSurface = wl_compositor_create_surface(app.wlCompositor);
    wl_surface_add_listener(wlSurface, &wlSurfaceLis, this);
    xdgSurface = xdg_wm_base_get_xdg_surface(app.xdgWmBase, wlSurface);
    xdg_surface_add_listener(xdgSurface, &xdgSurfaceLis, this);
    xdgToplevel = xdg_surface_get_toplevel(xdgSurface);
    xdg_toplevel_add_listener(xdgToplevel, &xdgToplevelLis, this);
    wl_surface_commit(wlSurface);
    swapchain = RWLSwapchain::Make(wlSurface, size);
}

static wl_callback_listener wlCallbackLis
{
    .done = [](void *data, wl_callback *wlCallback, uint32_t)
    {
        Window &window { *static_cast<Window*>(data) };
        window.wlCallback = nullptr;
        window.update();
        wl_callback_destroy(wlCallback);
    }
};

void Window::update() noexcept
{
    if (!ready)
        return;

    SkISize bufferSize = { size.width() * scale, size.height() * scale };

    if (needsNewSurface)
    {
        needsNewSurface = false;
        swapchain->resize(bufferSize);
    }

    wl_surface_set_buffer_scale(wlSurface, scale);

    auto image { swapchain->acquire() };
    assert(image);

    RLog(CZInfo, "Age: {}", image.value().age);
    auto surface = RSurface::WrapImage(image.value().image);
    assert(surface);

    RSurfaceGeometry geo { surface->geometry() };
    geo.viewport = SkRect::Make(size);
    surface->setGeometry(geo);

    auto pass { surface->beginPass() };
    assert(pass);
    auto p { pass->getPainter() };

    RDrawImageInfo info {};
    info.image = testImage;
    info.srcScale = 1.f;
    info.srcTransform = CZTransform::Normal;
    info.src = SkRect::MakeXYWH(
        0,
        0,
        testImage->size().width(),
        testImage->size().height());
    info.dst = SkIRect::MakeXYWH(10, 10, size.width() - 20, size.height() - 20);
    p->drawImage(info);
    p->setColor(SK_ColorCYAN);
    p->drawColor(SkRegion(SkIRect::MakeXYWH(10, 10, 40, 40)));

    if (!wlCallback)
    {
        wlCallback = wl_surface_frame(wlSurface);
        wl_callback_add_listener(wlCallback, &wlCallbackLis ,this);
    }

    // Animate window size
    static SkScalar f { 0.f }; f += 0.1f;
    size.fWidth = 500 + 250 * SkScalarCos(f);
    size.fHeight = 500 + 250 * SkScalarSin(f);
    needsNewSurface = true;

    // End pass
    pass.reset();

    swapchain->present(image.value());
}

static xdg_wm_base_listener xdgWmBaseLis
{
    .ping = [](void */*data*/, xdg_wm_base *xdgWmBase, uint32_t serial) { xdg_wm_base_pong(xdgWmBase, serial); }
};

static wl_registry_listener wlRegistryLis
{
    .global = [](void */*data*/, wl_registry *wl_registry, uint32_t name, const char *interface, uint32_t version)
    {
        if (version >= 6 && !app.wlCompositor && strcmp(interface, wl_compositor_interface.name) == 0)
            app.wlCompositor = (wl_compositor*)wl_registry_bind(wl_registry, name, &wl_compositor_interface, 6);
        else if (!app.xdgWmBase && strcmp(interface, xdg_wm_base_interface.name) == 0)
        {
            app.xdgWmBase = (xdg_wm_base*)wl_registry_bind(wl_registry, name, &xdg_wm_base_interface, 1);
            xdg_wm_base_add_listener(app.xdgWmBase, &xdgWmBaseLis, NULL);
        }
    },
        .global_remove = [](auto, auto, auto){}
};

int main()
{

    setenv("CZ_REAM_LOG_LEVEL", "6", 0);
    setenv("CZ_REAM_EGL_LOG_LEVEL", "6", 0);

    app.wlDisplay = wl_display_connect(NULL);

    if (!app.wlDisplay)
    {
        RLog(CZFatal, CZLN, "Failed to create wl_display");
        return EXIT_FAILURE;
    }

    app.wlRegistry = wl_display_get_registry(app.wlDisplay);
    wl_registry_add_listener(app.wlRegistry, &wlRegistryLis, NULL);
    wl_display_roundtrip(app.wlDisplay);
    assert("Failed to get wl_compositor v6" && app.wlCompositor);
    assert("Failed to get xdg_wm_base" && app.wlCompositor);

    RCore::Options options {};
    options.graphicsAPI = RGraphicsAPI::GL;
    options.platformHandle = RWLPlatformHandle::Make(app.wlDisplay);
    auto core = RCore::Make(options);

    auto buff = std::vector<UInt8>();
    buff.resize(100*100*4);
    for (size_t x = 0; x < 100; x++)
    {
        for (size_t y = 0; y < 100; y++)
        {
            size_t i = x * 4 + (100 * y * 4);

            if (y < 50)
            {
                if (x < 50) {
                    buff[i++] = 255; buff[i++] = 0;   buff[i++] = 0; buff[i++] = 255;
                } else {
                    buff[i++] = 0;   buff[i++] = 255; buff[i++] = 0; buff[i++] = 255;
                }
            }
            else
            {
                if (x < 50) {
                    buff[i++] = 0;   buff[i++] = 0;   buff[i++] = 255;  buff[i++] = 255;
                } else {
                    buff[i++] = 255; buff[i++] = 255; buff[i++] = 255;  buff[i++] = 255;
                }
            }
        }
    }

    RPixelBufferInfo info {};
    info.size.set(100, 100);
    info.format = DRM_FORMAT_ABGR8888;
    info.stride = 100 * 4;
    info.pixels = buff.data();

    testImage = RImage::Make({100, 100}, { DRM_FORMAT_ABGR8888, { DRM_FORMAT_MOD_INVALID } });
    assert(testImage);
    assert(testImage->writePixels({
        .offset = {0, 0},
        .stride = 100 * 4,
        .pixels = buff.data(),
        .region = SkRegion(SkIRect::MakeWH(100, 100)),
        .format = DRM_FORMAT_ABGR8888,
    }));

    Window win {};

    for (int i = 0; i < 60 * 5; i++)
    {
        wl_display_dispatch(app.wlDisplay);
        core->clearGarbage();
    }

    return 0;
}
