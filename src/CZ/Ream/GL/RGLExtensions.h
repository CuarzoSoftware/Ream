#ifndef RGLEXTENSIONS_H
#define RGLEXTENSIONS_H

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

namespace CZ
{
    struct REGLClientExtensions
    {
        bool EXT_platform_base;
        bool MESA_platform_gbm;
        bool KHR_platform_gbm;
        bool KHR_platform_wayland;
        bool EXT_device_base;
        bool EXT_device_query;
        bool KHR_debug;
    };

    struct REGLDisplayExtensions
    {
        bool KHR_image;
        bool KHR_image_base;
        bool EXT_image_dma_buf_import;
        bool EXT_image_dma_buf_import_modifiers;
        bool KHR_image_pixmap;
        bool KHR_gl_texture_2D_image;
        bool KHR_gl_renderbuffer_image;
        bool KHR_no_config_context;
        bool MESA_configless_context;
        bool KHR_surfaceless_context;
        bool IMG_context_priority;
        bool KHR_fence_sync;
        bool KHR_wait_sync;
        bool ANDROID_native_fence_sync;
    };

    struct REGLDeviceExtensions
    {
        bool EXT_device_drm_render_node;
    };

    struct RGLExtensions
    {
        bool EXT_read_format_bgra;
        bool EXT_texture_format_BGRA8888;
        bool OES_EGL_image;
        bool OES_EGL_image_base;
        bool OES_EGL_image_external;
        bool OES_EGL_sync;
        bool OES_surfaceless_context;
    };

    struct REGLClientProcs
    {
        PFNEGLGETPLATFORMDISPLAYEXTPROC eglGetPlatformDisplayEXT;
        PFNEGLQUERYDEVICESTRINGEXTPROC eglQueryDeviceStringEXT;
        PFNEGLQUERYDISPLAYATTRIBEXTPROC eglQueryDisplayAttribEXT;
        PFNEGLDEBUGMESSAGECONTROLKHRPROC eglDebugMessageControlKHR;
    };

    struct REGLDisplayProcs
    {
        PFNEGLCREATEIMAGEKHRPROC eglCreateImageKHR;
        PFNEGLDESTROYIMAGEKHRPROC eglDestroyImageKHR;
        PFNEGLQUERYDMABUFFORMATSEXTPROC eglQueryDmaBufFormatsEXT;
        PFNEGLQUERYDMABUFMODIFIERSEXTPROC eglQueryDmaBufModifiersEXT;
        PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES;
        PFNGLEGLIMAGETARGETRENDERBUFFERSTORAGEOESPROC glEGLImageTargetRenderbufferStorageOES;
        PFNEGLCREATESYNCKHRPROC eglCreateSyncKHR;
        PFNEGLDESTROYSYNCKHRPROC eglDestroySyncKHR;
        PFNEGLWAITSYNCKHRPROC eglWaitSyncKHR;
        PFNEGLDUPNATIVEFENCEFDANDROIDPROC eglDupNativeFenceFDANDROID;
    };
};

#endif // RGLEXTENSIONS_H
