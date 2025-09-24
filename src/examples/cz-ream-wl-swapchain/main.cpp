#include <WL/RWLPlatformHandle.h>
#include <WL/RWLSwapchain.h>
#include <RSurface.h>
#include <RPainter.h>
#include <RPass.h>
#include <RCore.h>
#include <RLog.h>

#include <wayland-client.h>
#include <wayland-egl.h>
#include "xdg-shell.h"

using namespace CZ;

struct App
{
    App() noexcept;
    ~App() noexcept;

    /// Render a frame if the surface is configured.
    void render() noexcept;

    struct
    {
        wl_display    *display;
        wl_registry   *registry;
        wl_compositor *compositor;
        xdg_wm_base   *xdgWmBase;
        wl_callback   *callback;
        wl_surface    *surface;
        xdg_surface   *xdgSurface;
        xdg_toplevel  *xdgToplevel;
    } wl {};

    // Ream core + swapchain
    std::shared_ptr<RCore> core;
    std::shared_ptr<RWLSwapchain> swapchain;

    // Window state
    SkISize winSize    { 512, 512 };
    Int32   winScale   { 1 };
    bool    configured { false };
    bool    running    { true };
};

// ------------------- Wayland Listeners -------------------

static wl_surface_listener WLSurfaceListener
{
    .enter = [](auto, auto, auto) {},
    .leave = [](auto, auto, auto) {},
    .preferred_buffer_scale = [](void *data, auto, Int32 factor)
    {
        auto *app = static_cast<App*>(data);
        app->winScale = factor;
        wl_surface_set_buffer_scale(app->wl.surface, factor);
    },
    .preferred_buffer_transform = [](auto, auto, auto) {}
};

static xdg_surface_listener XDGSurfaceListener
{
    .configure = [](void *data, xdg_surface *xdgSurface, UInt32 serial)
    {
        auto *app = static_cast<App*>(data);
        xdg_surface_ack_configure(xdgSurface, serial);
        app->configured = true;
        app->render();
    }
};

static xdg_toplevel_listener XDGToplevelListener
{
    .configure = [](void *data, auto, Int32 width, Int32 height, auto)
    {
        auto *app = static_cast<App*>(data);

        // Enforce a minimum window size
        if (width  < 256) width  = 256;
        if (height < 256) height = 256;

        app->winSize.set(width, height);
    },
    .close = [](void *data, auto)
    {
        static_cast<App*>(data)->running = false;
    },
    .configure_bounds = [](auto, auto, auto, auto) {},
    .wm_capabilities  = [](auto, auto, auto) {}
};

static wl_callback_listener WLCallbackListener
{
    .done = [](void *data, wl_callback *callback, auto)
    {
        auto *app = static_cast<App*>(data);
        app->wl.callback = nullptr;
        app->render();
        wl_callback_destroy(callback);
    }
};

static xdg_wm_base_listener XDGWmBaseListener
{
    .ping = [](auto, xdg_wm_base *xdgWmBase, UInt32 serial)
    {
        xdg_wm_base_pong(xdgWmBase, serial);
    }
};

static wl_registry_listener WLRegistryListener
{
    .global = [](void *data, wl_registry *registry,
                 UInt32 name, const char *interface, UInt32 version)
    {
        auto *app = static_cast<App*>(data);

        if (!app->wl.compositor &&
            strcmp(interface, wl_compositor_interface.name) == 0 &&
            version >= 6)
        {
            app->wl.compositor = static_cast<wl_compositor*>(
                wl_registry_bind(registry, name, &wl_compositor_interface, 6));
        }
        else if (!app->wl.xdgWmBase &&
                 strcmp(interface, xdg_wm_base_interface.name) == 0)
        {
            app->wl.xdgWmBase = static_cast<xdg_wm_base*>(
                wl_registry_bind(registry, name, &xdg_wm_base_interface, 1));
            xdg_wm_base_add_listener(app->wl.xdgWmBase, &XDGWmBaseListener, app);
        }
    },
    .global_remove = [](auto, auto, auto) {}
};

// ------------------- App Implementation -------------------

App::App() noexcept
{
    // Connect to the Wayland display
    wl.display = wl_display_connect(nullptr);
    assert(wl.display && "wl_display_connect failed");

    // Fetch and bind globals
    wl.registry = wl_display_get_registry(wl.display);
    wl_registry_add_listener(wl.registry, &WLRegistryListener, this);
    wl_display_roundtrip(wl.display);

    assert(wl.compositor && "wl_compositor v6 not found");
    assert(wl.xdgWmBase  && "xdg_wm_base not found");

    // Initialize Ream
    RCore::Options options {};
    options.graphicsAPI    = RGraphicsAPI::Auto;
    options.platformHandle = RWLPlatformHandle::Make(wl.display, CZOwn::Own);
    core = RCore::Make(options);
    assert(core && "Failed to create RCore");

    // Create window surface + toplevel
    wl.surface     = wl_compositor_create_surface(wl.compositor);
    wl_surface_add_listener(wl.surface, &WLSurfaceListener, this);

    wl.xdgSurface  = xdg_wm_base_get_xdg_surface(wl.xdgWmBase, wl.surface);
    xdg_surface_add_listener(wl.xdgSurface, &XDGSurfaceListener, this);

    wl.xdgToplevel = xdg_surface_get_toplevel(wl.xdgSurface);
    xdg_toplevel_add_listener(wl.xdgToplevel, &XDGToplevelListener, this);

    wl_surface_commit(wl.surface);
}

App::~App() noexcept
{
    // Destroy in reverse creation order
    xdg_toplevel_destroy(wl.xdgToplevel);
    xdg_surface_destroy(wl.xdgSurface);

    // Must be destroyed before the wl_surface
    swapchain.reset();
    wl_surface_destroy(wl.surface);

    if (wl.callback)
        wl_callback_destroy(wl.callback);

    xdg_wm_base_destroy(wl.xdgWmBase);
    wl_compositor_destroy(wl.compositor);
    wl_registry_destroy(wl.registry);

    core.reset();
}

/// Perform a rendering pass.
void App::render() noexcept
{
    if (!configured)
        return;

    SkISize bufferSize { winSize.width() * winScale, winSize.height() * winScale };

    // Create the swapchain lazily, once we know buffer size
    if (!swapchain)
    {
        swapchain = RWLSwapchain::Make(wl.surface, bufferSize);
        assert(swapchain && "Failed to create swapchain");
    }

    // Resize swapchain if needed
    if (swapchain->size() != bufferSize)
        swapchain->resize(bufferSize);

    // Acquire an image for rendering
    auto swapchainImage = swapchain->acquire();
    assert(swapchainImage && "Failed to acquire swapchain image");

    // Log buffer reuse age
    RLog(CZInfo, "Swapchain Image Age: {}", swapchainImage->age);

    auto surface = RSurface::WrapImage(swapchainImage->image);
    assert(surface && "Failed to wrap swapchain image");

    // Match viewport to window size (logical size => buffer size)
    RSurfaceGeometry geo = surface->geometry();
    geo.viewport = SkRect::Make(winSize);
    surface->setGeometry(geo);

    // Begin render pass (only SkCanvas in this example)
    auto pass = surface->beginPass(RPassCap_SkCanvas);
    assert(pass && "Failed to begin pass");

    SkCanvas *canvas = pass->getCanvas();
    canvas->clear(SK_ColorWHITE);

    // Draw a frame
    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setColor(SK_ColorBLACK);
    paint.setStroke(true);
    paint.setStrokeWidth(20);
    canvas->drawRect(geo.viewport, paint);

    // Draw a red circle centered in the window
    static float phase { 0.f };
    phase += 0.05f;
    const auto rad { 0.2f * (SkScalarCos(phase) + 1.f) };
    paint.setStroke(false);
    paint.setColor(SK_ColorRED);
    canvas->drawCircle(
        SkPoint::Make(winSize.width() / 2, winSize.height() / 2),
        std::min(winSize.width(), winSize.height()) * rad,
        paint);

    // Schedule next frame callback if none pending
    if (!wl.callback)
    {
        wl.callback = wl_surface_frame(wl.surface);
        wl_callback_add_listener(wl.callback, &WLCallbackListener, this);
    }

    // End pass before presenting
    pass.reset();

    // Present to compositor
    swapchain->present(*swapchainImage);
}

// ------------------- Entry Point -------------------

int main()
{
    // Configure Ream via environment
    setenv("CZ_REAM_GAPI", "GL", 0); // Options: "RS", "GL", "VK"
    setenv("CZ_REAM_LOG_LEVEL", "6", 0);
    setenv("CZ_REAM_EGL_LOG_LEVEL", "6", 0);

    App app {};

    // Event loop
    while (wl_display_dispatch(app.wl.display) >= 0 && app.running)
        app.core->clearGarbage();

    return 0;
}
