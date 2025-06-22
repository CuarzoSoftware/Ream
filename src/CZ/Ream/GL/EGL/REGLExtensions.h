#ifndef REGLEXTENSIONS_H
#define REGLEXTENSIONS_H

#include <EGL/egl.h>
#include <EGL/eglext.h>

namespace CZ
{
    struct REGLInstanceExtensions
    {
        bool EXT_platform_base;
        bool KHR_platform_gbm;
        bool MESA_platform_gbm;
        bool EXT_platform_device;
        bool KHR_display_reference;
        bool EXT_device_base;
        bool EXT_device_enumeration;
        bool EXT_device_query;
        bool KHR_debug;
    };

    struct REGLDeviceExtensions
    {
        bool KHR_image;
        bool KHR_image_base;
        bool EXT_image_dma_buf_import;
        bool EXT_image_dma_buf_import_modifiers;
        bool EXT_create_context_robustness;
        bool MESA_device_software;
        bool KHR_image_pixmap;
        bool KHR_gl_texture_2D_image;
        bool KHR_gl_renderbuffer_image;
#ifdef EGL_DRIVER_NAME_EXT
        bool EXT_device_persistent_id;
#endif
        bool EXT_device_drm;
        bool EXT_device_drm_render_node;
        bool KHR_no_config_context;
        bool MESA_configless_context;
        bool KHR_surfaceless_context;
        bool IMG_context_priority;
        bool KHR_fence_sync;
        bool KHR_wait_sync;
        bool ANDROID_native_fence_sync;
    };
};

#endif // REGLEXTENSIONS_H
