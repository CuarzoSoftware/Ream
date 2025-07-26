#include <WL/RWLPlatformHandle.h>
#include <RLog.h>
#include <GL/RGLCore.h>
#include <GL/RGLDevice.h>
#include <GL/RGLMakeCurrent.h>
#include <RImage.h>
#include <RSurface.h>
#include <RPainter.h>
#include <RPass.h>

#include <GL/RGLImage.h>


#include <thread>
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
    EGLConfig eglConfig;
} app;

struct Window
{
    Window() noexcept;
    void update() noexcept;
    ~Window() noexcept
    {
        xdg_toplevel_destroy(xdgToplevel);
        xdg_surface_destroy(xdgSurface);
        wl_egl_window_destroy(wlEGLWindow);
        wl_surface_destroy(wlSurface);
    }


    //wl_callback *wlCallback { nullptr };
    wl_surface *wlSurface;
    xdg_surface *xdgSurface;
    xdg_toplevel *xdgToplevel;
    wl_egl_window *wlEGLWindow { nullptr };
    EGLSurface eglSurface;

    SkISize size;
    SkISize bufferSize;
    int32_t scale { 1 };
    bool needsNewSurface { true };
    bool ready { false };

    std::shared_ptr<RSurface> surf;
    std::thread::id thread { std::this_thread::get_id() };


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
        //window.update();
        window.ready = true;
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
}

static wl_callback_listener wlCallbackLis
{
    .done = [](void *data, wl_callback *wlCallback, uint32_t)
    {
        /*
        Window &window { *static_cast<Window*>(data) };
        window.wlCallback = nullptr;
        window.update();
        wl_callback_destroy(wlCallback);*/
    }
};

void Window::update() noexcept
{
    if (!ready)
        return;

    auto core { RGLCore::Get()->asGL() };
    auto dev { core->mainDevice() };

    bufferSize = { size.width() * scale, size.height() * scale };

    auto current1 = RGLMakeCurrent::FromDevice(dev, false);

    if (!wlEGLWindow)
    {
        wlEGLWindow = wl_egl_window_create(wlSurface, bufferSize.width(), bufferSize.height());
        eglSurface = eglCreateWindowSurface(dev->eglDisplay(), app.eglConfig, (EGLNativeWindowType)wlEGLWindow, NULL);
        assert("Failed to create EGLSurface" && eglSurface != EGL_NO_SURFACE);
        eglMakeCurrent(dev->eglDisplay(), eglSurface, eglSurface, dev->eglContext());
        eglSwapInterval(dev->eglDisplay(), 0);
    }

    auto current = RGLMakeCurrent(dev->eglDisplay(), eglSurface, eglSurface, dev->eglContext());

    if (needsNewSurface)
    {
        needsNewSurface = false;
        wl_egl_window_resize(wlEGLWindow, bufferSize.width(), bufferSize.height(), 0, 0);
        RGLFramebufferInfo info {};
        info.size = bufferSize;
        info.format = DRM_FORMAT_ARGB8888;
        info.id = 0;

        surf = RSurface::WrapImage(RGLImage::BorrowFramebuffer(info, dev), scale);

        /*
        const GrGLFramebufferInfo fbInfo
            {
                0,
                GL_RGBA8,
                skgpu::Protected::kNo
            };

        const GrBackendRenderTarget backendTarget = GrBackendRenderTargets::MakeGL(
            bufferSize.width(),
            bufferSize.height(),
            0, 0,
            fbInfo);

        skSurface = SkSurfaces::WrapBackendRenderTarget(
            akApp()->glContext()->skContext().get(),
            backendTarget,
            GrSurfaceOrigin::kBottomLeft_GrSurfaceOrigin,
            SkColorType::kRGBA_8888_SkColorType,
            SkColorSpace::MakeSRGB(),
            &skSurfaceProps);

        assert("Failed to create SkSurface" && skSurface.get());*/
    }

    wl_surface_set_buffer_scale(wlSurface, scale);

    glViewport(0, 0, bufferSize.width(), bufferSize.height());
    glScissor(0, 0, bufferSize.width(), bufferSize.height());
    static float f { 0.f };
    f += 0.05f;
    glClearColor(SkScalarCos(f) * 0.5f + 0.5f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    if (surf)
    {
        auto pass { surf->beginPass() };

        RDrawImageInfo info {};
        info.image = testImage;
        info.srcScale = 1.f;
        info.srcTransform = CZTransform::Normal;
        info.src = SkRect::MakeXYWH(
            0,
            0,
            testImage->size().width(),
            testImage->size().height());
        info.dst = SkIRect::MakeXYWH(10, 10, 256, 256);
        pass()->drawImage(info);
        pass()->setColor(SK_ColorCYAN);
        pass()->drawColor(SkRegion(SkIRect::MakeXYWH(10, 10, 40, 40)));
        pass.end();
    }
    else
        RLog(CZError, "NO SURF");

        /*
    if (!wlCallback)
    {
        //std::cout << "wl_surface::frame" << std::endl;
        wlCallback = wl_surface_frame(wlSurface);
        wl_callback_add_listener(wlCallback, &wlCallbackLis ,this);
    }*/

    eglSwapBuffers(dev->eglDisplay(), eglSurface);
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

static void initEGL() noexcept
{
    auto core { RGLCore::Get()->asGL() };
    auto dev { core->mainDevice() };

    EGLint numConfigs;
    assert("Failed to get EGL configurations." && eglGetConfigs(dev->eglDisplay(), NULL, 0, &numConfigs) == EGL_TRUE && numConfigs > 0);

    const EGLint fbAttribs[]
    {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE,        8,
        EGL_GREEN_SIZE,      8,
        EGL_BLUE_SIZE,       8,
        EGL_ALPHA_SIZE,      8,
        EGL_NONE
    };

    assert("Failed to choose EGL configuration." &&
           eglChooseConfig(dev->eglDisplay(), fbAttribs, &app.eglConfig, 1, &numConfigs) == EGL_TRUE && numConfigs == 1);

    eglMakeCurrent(dev->eglDisplay(), EGL_NO_SURFACE, EGL_NO_SURFACE, dev->eglContext());
}

static std::mutex mutex;
static std::shared_ptr<RCore> core;

void thread()
{
    Window win {};

    for (int i = 0; i < 50; i++)
    {
        mutex.lock();
        wl_display_dispatch(app.wlDisplay);
        win.update();
        core->clearGarbage();
        mutex.unlock();
        usleep(50000);
    }
}

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
    core = RCore::Make(options);
    initEGL();

    auto buff = std::vector<UInt8>();
    buff.resize(100*100*4);
    for (size_t x = 0; x < 100; x++)
    {
        for (size_t y = 0; y < 100; y++)
        {
            size_t i = x * 4 + (100 * y * 4);

            if (y < 50)
            {
                if (x < 50)
                {
                    buff[i++] = 255;
                    buff[i++] = 0;
                    buff[i++] = 0;
                    buff[i++] = 255;
                }
                else
                {
                    buff[i++] = 0;
                    buff[i++] = 255;
                    buff[i++] = 0;
                    buff[i++] = 255;
                }
            }
            else
            {
                if (x < 50)
                {
                    buff[i++] = 0;
                    buff[i++] = 0;
                    buff[i++] = 255;
                    buff[i++] = 255;
                }
                else
                {
                    buff[i++] = 255;
                    buff[i++] = 255;
                    buff[i++] = 255;
                    buff[i++] = 255;
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

    std::thread([]{
        thread();
    }).detach();

    Window win {};

    for (int i = 0; i < 100; i++)
    {
        mutex.lock();
        wl_display_dispatch(app.wlDisplay);
        win.update();
        core->clearGarbage();
        mutex.unlock();
        usleep(50000);
    }

    return 0;
}
