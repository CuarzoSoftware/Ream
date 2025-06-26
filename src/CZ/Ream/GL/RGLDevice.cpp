#include "Utils/CZStringUtils.h"
#include <CZ/Ream/WL/RWLPlatformHandle.h>
#include <CZ/Ream/GL/RGLDevice.h>
#include <CZ/Ream/GL/RGLCore.h>
#include <CZ/Ream/RLog.h>
#include <fcntl.h>
#include <gbm.h>

using namespace CZ;

RGLDevice *RGLDevice::Make(RGLCore &core, int drmFd) noexcept
{
    auto dev { new RGLDevice(core, drmFd)};

    if (dev->init())
        return dev;

    delete dev;
    return nullptr;
}

RGLDevice::RGLDevice(RGLCore &core, int drmFd) noexcept :
    RDevice(core)
{
    m_drmFd = drmFd;
}

RGLDevice::~RGLDevice()
{
    if (eglContext() != EGL_NO_CONTEXT)
    {
        eglMakeCurrent(eglDisplay(), EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroyContext(eglDisplay(), eglContext());
        m_eglContext = EGL_NO_CONTEXT;
    }

    if (eglDisplay() != EGL_NO_DISPLAY)
    {
        eglTerminate(eglDisplay());
        m_eglDisplay = EGL_NO_DISPLAY;
    }

    if (gbmDevice())
    {
        gbm_device_destroy(m_gbmDevice);
        m_gbmDevice = nullptr;
    }

    if (core().platform() == RPlatform::Wayland)
    {
        if (drmFd())
        {
            close(drmFd());
            m_drmFd = -1;
        }
    }
}

bool RGLDevice::init() noexcept
{
    if (core().platform() == RPlatform::Wayland)
        return initWL();

    return false;
}

bool RGLDevice::initWL() noexcept
{
    if (initEGLDisplayWL() &&
        initEGLDisplayExtensions() &&
        initEGLContext() &&
        initGLExtensions() &&
        initEGLDisplayProcs())
        return true;

    return false;
}

bool RGLDevice::initEGLDisplayWL() noexcept
{
    // KHR_platform_wayland already validated
    m_eglDisplay = core().clientEGLProcs().eglGetPlatformDisplayEXT(EGL_PLATFORM_WAYLAND_KHR, core().options().platformHandle->asWL()->wlDisplay(), NULL);

    if (eglDisplay() == EGL_NO_DISPLAY)
    {
        RError(RLINE, "Failed to get EGL display.");
        return false;
    }

    EGLint minor, major;

    if (!eglInitialize(eglDisplay(), &minor, &major))
    {
        RError(RLINE, "Failed to initialize EGL display.");
        m_eglDisplay = EGL_NO_DISPLAY;
        return false;
    }

    /* Below this point everything is optional (for GBM allocator support) */

    if (!core().clientEGLExtensions().EXT_device_query)
    {
        RWarning(RLINE, "EGL_EXT_device_query extension not available. GBM allocator disabled.");
        return true;
    }

    EGLAttrib deviceAttrib;

    if (!core().clientEGLProcs().eglQueryDisplayAttribEXT(eglDisplay(), EGL_DEVICE_EXT, &deviceAttrib))
    {
        RWarning(RLINE, "Failed to get EGLDevice from EGLDisplay. GBM allocator disabled.");
        return true;
    }

    m_eglDevice = (EGLDeviceEXT)deviceAttrib;

    const char *deviceExtensions { core().clientEGLProcs().eglQueryDeviceStringEXT(eglDevice(), EGL_EXTENSIONS) };
    m_eglDeviceExtensions.EXT_device_drm_render_node = CZStringUtils::CheckExtension(deviceExtensions, "EGL_EXT_device_drm_render_node");

    if (!eglDeviceExtensions().EXT_device_drm_render_node)
    {
        RWarning(RLINE, "EGL_EXT_device_drm_render_node extension not available. GBM allocator disabled.");
        return true;
    }

    const char *nodeName = core().clientEGLProcs().eglQueryDeviceStringEXT(eglDevice(), EGL_DRM_RENDER_NODE_FILE_EXT);

    if (!nodeName)
    {
        RWarning(RLINE, "Failed to get DRM node name. GBM allocator disabled.");
        return true;
    }

    const int drmFd { open(nodeName, O_RDWR | O_CLOEXEC | O_NONBLOCK) };

    if (drmFd < 0)
    {
        RWarning(RLINE, "Failed to open DRM node %s. GBM allocator disabled.", nodeName);
        return true;
    }

    gbm_device *gbm { gbm_create_device(drmFd) };

    if (!gbm)
    {
        RWarning(RLINE, "Failed to create gbm_device from DRM node %s. GBM allocator disabled.", nodeName);
        goto failGBM;
    }

    m_drmFd = drmFd;
    m_gbmDevice = gbm;
    m_drmNode = nodeName;
    return true;
failGBM:
    close(drmFd);
    return true;
}

bool RGLDevice::initEGLDisplayExtensions() noexcept
{
    const char *extensions { eglQueryString(eglDisplay(), EGL_EXTENSIONS) };

    if (!extensions)
    {
        RError(RLINE, "Failed to query EGL display extensions.");
        return false;
    }

    auto &exts { m_eglDisplayExtensions };

    exts.KHR_no_config_context = CZStringUtils::CheckExtension(extensions, "EGL_KHR_no_config_context");
    exts.MESA_configless_context = CZStringUtils::CheckExtension(extensions, "EGL_MESA_configless_context");

    if (!exts.KHR_no_config_context && !exts.MESA_configless_context)
    {
        RError(RLINE, "Required EGL extensions EGL_KHR_no_config_context and EGL_MESA_configless_context are not available.");
        return false;
    }

    exts.KHR_surfaceless_context = CZStringUtils::CheckExtension(extensions, "EGL_KHR_surfaceless_context");

    if (!exts.KHR_surfaceless_context)
    {
        RError(RLINE, "Required EGL extension KHR_surfaceless_context is not available.");
        return false;
    }

    exts.KHR_image_base = CZStringUtils::CheckExtension(extensions, "EGL_KHR_image_base");
    exts.KHR_image = CZStringUtils::CheckExtension(extensions, "EGL_KHR_image");
    exts.EXT_image_dma_buf_import = CZStringUtils::CheckExtension(extensions, "EGL_EXT_image_dma_buf_import");
    exts.EXT_image_dma_buf_import_modifiers = CZStringUtils::CheckExtension(extensions, "EGL_EXT_image_dma_buf_import_modifiers");
    exts.KHR_image_pixmap = CZStringUtils::CheckExtension(extensions, "EGL_KHR_image_pixmap");
    exts.KHR_gl_texture_2D_image = CZStringUtils::CheckExtension(extensions, "EGL_KHR_gl_texture_2D_image");
    exts.KHR_gl_renderbuffer_image = CZStringUtils::CheckExtension(extensions, "EGL_KHR_gl_renderbuffer_image");
    exts.KHR_wait_sync = CZStringUtils::CheckExtension(extensions, "EGL_KHR_wait_sync");
    exts.KHR_fence_sync = CZStringUtils::CheckExtension(extensions, "EGL_KHR_fence_sync");
    exts.ANDROID_native_fence_sync = CZStringUtils::CheckExtension(extensions, "EGL_ANDROID_native_fence_sync");
    exts.IMG_context_priority = CZStringUtils::CheckExtension(extensions, "EGL_IMG_context_priority");
    return true;
}

bool RGLDevice::initEGLContext() noexcept
{
    if (!eglBindAPI(EGL_OPENGL_ES_API))
    {
        RError(RLINE, "Failed to bind OpenGL ES API.");
        return false;
    }

    size_t atti { 0 };
    EGLint attribs[5];
    attribs[atti++] = EGL_CONTEXT_CLIENT_VERSION;
    attribs[atti++] = 2;

    if (core().platform() == RPlatform::DRM && eglDisplayExtensions().IMG_context_priority)
    {
        attribs[atti++] = EGL_CONTEXT_PRIORITY_LEVEL_IMG;
        attribs[atti++] = EGL_CONTEXT_PRIORITY_HIGH_IMG;
    }

    attribs[atti++] = EGL_NONE;

    m_eglContext = eglCreateContext(eglDisplay(), EGL_NO_CONFIG_KHR, EGL_NO_CONTEXT, attribs);

    if (eglContext() == EGL_NO_CONTEXT)
    {
        RError(RLINE, "Failed to create shared EGL context.");
        return 0;
    }

    eglMakeCurrent(eglDisplay(), EGL_NO_SURFACE, EGL_NO_SURFACE, eglContext());
    return true;
}

bool RGLDevice::initGLExtensions() noexcept
{
    const char *extensions { (const char*)glGetString(GL_EXTENSIONS) };
    auto &glExts { m_glExtensions };
    glExts.EXT_read_format_bgra = CZStringUtils::CheckExtension(extensions, "GL_EXT_read_format_bgra");
    glExts.EXT_texture_format_BGRA8888 = CZStringUtils::CheckExtension(extensions, "GL_EXT_texture_format_BGRA8888");
    glExts.OES_EGL_image_external = CZStringUtils::CheckExtension(extensions, "GL_OES_EGL_image_external");
    glExts.OES_EGL_image = CZStringUtils::CheckExtension(extensions, "GL_OES_EGL_image");
    glExts.OES_EGL_image_base = CZStringUtils::CheckExtension(extensions, "GL_OES_EGL_image_base");
    glExts.OES_surfaceless_context = CZStringUtils::CheckExtension(extensions, "GL_OES_surfaceless_context");
    glExts.OES_EGL_sync = CZStringUtils::CheckExtension(extensions, "GL_OES_EGL_sync");
    return true;
}

bool RGLDevice::initEGLDisplayProcs() noexcept
{
    auto &procs { m_eglDisplayProcs };
    auto &exts { m_eglDisplayExtensions };
    auto &glExts { m_glExtensions };

    if (exts.KHR_image_base || exts.KHR_image)
    {
        procs.eglCreateImageKHR = (PFNEGLCREATEIMAGEKHRPROC) eglGetProcAddress("eglCreateImageKHR");
        procs.eglDestroyImageKHR = (PFNEGLDESTROYIMAGEKHRPROC) eglGetProcAddress("eglDestroyImageKHR");

        if (glExts.OES_EGL_image || glExts.OES_EGL_image_base)
        {
            if (exts.KHR_gl_texture_2D_image)
                procs.glEGLImageTargetTexture2DOES = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC) eglGetProcAddress("glEGLImageTargetTexture2DOES");

            if (exts.KHR_gl_renderbuffer_image)
                procs.glEGLImageTargetRenderbufferStorageOES = (PFNGLEGLIMAGETARGETRENDERBUFFERSTORAGEOESPROC) eglGetProcAddress("glEGLImageTargetRenderbufferStorageOES");
        }
    }

    const UInt8 hasEGLSync = glExts.OES_EGL_sync &&
                             exts.KHR_fence_sync &&
                             exts.KHR_wait_sync &&
                             exts.ANDROID_native_fence_sync;

    if (hasEGLSync)
    {
        procs.eglCreateSyncKHR = (PFNEGLCREATESYNCKHRPROC)eglGetProcAddress("eglCreateSyncKHR");
        procs.eglDestroySyncKHR = (PFNEGLDESTROYSYNCKHRPROC)eglGetProcAddress("eglDestroySyncKHR");
        procs.eglWaitSyncKHR = (PFNEGLWAITSYNCKHRPROC)eglGetProcAddress("eglWaitSyncKHR");
        procs.eglDupNativeFenceFDANDROID = (PFNEGLDUPNATIVEFENCEFDANDROIDPROC)eglGetProcAddress("eglDupNativeFenceFDANDROID");
    }

    if (exts.EXT_image_dma_buf_import_modifiers)
    {
        procs.eglQueryDmaBufFormatsEXT = (PFNEGLQUERYDMABUFFORMATSEXTPROC) eglGetProcAddress("eglQueryDmaBufFormatsEXT");
        procs.eglQueryDmaBufModifiersEXT = (PFNEGLQUERYDMABUFMODIFIERSEXTPROC) eglGetProcAddress("eglQueryDmaBufModifiersEXT");
    }

    return true;
}
