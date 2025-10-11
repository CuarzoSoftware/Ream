#include <CZ/Ream/WL/RWLPlatformHandle.h>
#include <CZ/Ream/SK/RSKContext.h>
#include <CZ/Ream/GL/RGLDevice.h>
#include <CZ/Ream/GL/RGLCore.h>
#include <CZ/Ream/GL/RGLPainter.h>
#include <CZ/Ream/RLog.h>

#include <CZ/skia/gpu/ganesh/gl/GrGLAssembleInterface.h>
#include <CZ/skia/gpu/ganesh/gl/GrGLDirectContext.h>

#include <CZ/Core/Utils/CZStringUtils.h>

#include <fcntl.h>
#include <gbm.h>
#include <xf86drm.h>
#include <drm_fourcc.h>
#include <sys/stat.h>

using namespace CZ;

static auto skInterface = GrGLMakeAssembledInterface(nullptr, (GrGLGetProc)*[](void *, const char *p) -> void * {
    return (void *)eglGetProcAddress(p);
});

using namespace CZ;

RGLDevice *RGLDevice::Make(RGLCore &core, int drmFd, void *userData) noexcept
{
    auto dev { new RGLDevice(core, drmFd, userData)};

    if (dev->init())
    {
        const char *env { std::getenv("CZ_REAM_EGL_LOG_LEVEL") };

        if (env && static_cast<UInt32>(atoi(env)) >= CZTrace)
        {
            dev->log(CZDebug, "EGL DMA Texture Formats:");
            dev->m_dmaTextureFormats.log();

            dev->log(CZDebug, "EGL DMA Render Formats:");
            dev->m_dmaRenderFormats.log();
        }
        return dev;
    }

    delete dev;
    return nullptr;
}

RGLDevice::RGLDevice(RGLCore &core, int drmFd, void *userData) noexcept :
    RDevice(core)
{
    m_drmFd = drmFd;
    m_drmUserData = userData;
}

RGLDevice::~RGLDevice() noexcept
{
    if (m_eglContext != EGL_NO_CONTEXT)
    {
        eglMakeCurrent(eglDisplay(), EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroyContext(eglDisplay(), m_eglContext);
        m_eglContext = EGL_NO_CONTEXT;
    }

    if (eglDisplay() != EGL_NO_DISPLAY)
    {
        eglTerminate(eglDisplay());
        m_eglDisplay = EGL_NO_DISPLAY;
    }
}

bool RGLDevice::init() noexcept
{
    if (core().platform() == RPlatform::Wayland)
        return initWL();
    else
        return initDRM();

    return false;
}

bool RGLDevice::initWL() noexcept
{
    if (initEGLDisplayWL() &&
        initEGLDisplayExtensions() &&
        initEGLContext() &&
        initGLExtensions() &&
        initEGLDisplayProcs() &&
        initDMAFormats() &&
        initFormats() &&
        initPainter())
        return true;

    return false;
}

bool RGLDevice::initEGLDisplayWL() noexcept
{
    // KHR_platform_wayland already validated
    m_eglDisplay = core().clientEGLProcs().eglGetPlatformDisplayEXT(EGL_PLATFORM_WAYLAND_KHR, core().options().platformHandle->asWL()->wlDisplay(), NULL);

    if (eglDisplay() == EGL_NO_DISPLAY)
    {
        log(CZError, CZLN, "Failed to get EGL display");
        return false;
    }

    EGLint minor, major;

    if (!eglInitialize(eglDisplay(), &minor, &major))
    {
        log(CZError, CZLN, "Failed to initialize EGL display");
        m_eglDisplay = EGL_NO_DISPLAY;
        return false;
    }

    /* Below this point everything is optional (for GBM allocator support) */

    if (!core().clientEGLExtensions().EXT_device_query)
    {
        log(CZWarning, CZLN, "EGL_EXT_device_query extension not available. GBM allocator disabled");
        return true;
    }

    EGLAttrib deviceAttrib;

    if (!core().clientEGLProcs().eglQueryDisplayAttribEXT(eglDisplay(), EGL_DEVICE_EXT, &deviceAttrib))
    {
        log(CZWarning, CZLN, "Failed to get EGLDevice from EGLDisplay. GBM allocator disabled");
        return true;
    }

    m_eglDevice = (EGLDeviceEXT)deviceAttrib;

    const char *deviceExtensions { core().clientEGLProcs().eglQueryDeviceStringEXT(eglDevice(), EGL_EXTENSIONS) };
    m_eglDeviceExtensions.EXT_device_drm_render_node = CZStringUtils::CheckExtension(deviceExtensions, "EGL_EXT_device_drm_render_node");
    m_eglDeviceExtensions.EXT_device_drm = CZStringUtils::CheckExtension(deviceExtensions, "EGL_EXT_device_drm");

    if (eglDeviceExtensions().EXT_device_drm)
    {
        const char *primaryNode = core().clientEGLProcs().eglQueryDeviceStringEXT(eglDevice(), EGL_DRM_DEVICE_FILE_EXT);

        if (primaryNode)
        {
            m_drmFdReadOnly = open(primaryNode, O_RDONLY);

            if (m_drmFdReadOnly >= 0)
                setDRMDriverName(m_drmFdReadOnly);
        }
    }

    if (!eglDeviceExtensions().EXT_device_drm_render_node)
    {
        log(CZWarning, CZLN, "EGL_EXT_device_drm_render_node extension not available. GBM allocator disabled");
        return true;
    }

    const char *nodeName = core().clientEGLProcs().eglQueryDeviceStringEXT(eglDevice(), EGL_DRM_RENDER_NODE_FILE_EXT);

    if (!nodeName)
    {
        log = RLog.newWithContext("Unknown Device");
        log(CZWarning, CZLN, "Failed to get DRM node name. GBM allocator disabled");
        return true;
    }

    log = RLog.newWithContext(CZStringUtils::SubStrAfterLastOf(nodeName, "/"));

    const int drmFd { open(nodeName, O_RDWR | O_CLOEXEC | O_NONBLOCK) };

    if (drmFd < 0)
    {
        log(CZWarning, CZLN, "Failed to open DRM node {}. GBM allocator disabled", nodeName);
        return true;
    }

    struct stat stat;
    if (fstat(drmFd, &stat) == 0)
        m_id = stat.st_rdev;

    gbm_device *gbm { gbm_create_device(drmFd) };

    if (!gbm)
    {
        log(CZWarning, CZLN, "Failed to create gbm_device from DRM node {}. GBM allocator disabled", nodeName);
        return true;
    }

    m_drmFd = drmFd;
    m_gbmDevice = gbm;
    m_drmNode = nodeName;
    return true;
}

bool RGLDevice::initDRM() noexcept
{
    if (initEGLDisplayDRM() &&
        initEGLDisplayExtensions() &&
        initEGLContext() &&
        initGLExtensions() &&
        initEGLDisplayProcs() &&
        initDMAFormats() &&
        initFormats() &&
        initPainter())
        return true;

    return false;
}

bool RGLDevice::initEGLDisplayDRM() noexcept
{
    struct stat stat;
    if (fstat(m_drmFd, &stat) == 0)
        m_id = stat.st_rdev;

    drmDevicePtr device;

    if (drmGetDevice(m_drmFd, &device) == 0)
    {
        if (device->available_nodes & (1 << DRM_NODE_PRIMARY) && device->nodes[DRM_NODE_PRIMARY])
            m_drmNode = device->nodes[DRM_NODE_PRIMARY];
        else if (device->available_nodes & (1 << DRM_NODE_RENDER) && device->nodes[DRM_NODE_RENDER])
            m_drmNode = device->nodes[DRM_NODE_RENDER];

        drmFreeDevice(&device);
    }

    if (m_drmNode.empty())
        m_drmNode = "Unknown Device";
    else
    {
        m_drmFdReadOnly = open(m_drmNode.c_str(), O_RDONLY | O_CLOEXEC);
        log = RLog.newWithContext(CZStringUtils::SubStrAfterLastOf(m_drmNode, "/"));
    }

    setDRMDriverName(m_drmFd);

    m_gbmDevice = gbm_create_device(m_drmFd);

    if (!m_gbmDevice)
    {
        log(CZError, CZLN, "Failed to create gbm_device");
        return false;
    }

    UInt64 value {};
    drmGetCap(m_drmFd, DRM_CAP_ADDFB2_MODIFIERS, &value);
    m_caps.AddFb2Modifiers = value == 1;
    value = {};
    drmGetCap(m_drmFd, DRM_CAP_DUMB_BUFFER, &value);
    m_caps.DumbBuffer = value == 1;

    //KHR_platform_gbm or MESA_platform_gbm already validated
    m_eglDisplay = core().clientEGLProcs().eglGetPlatformDisplayEXT(EGL_PLATFORM_GBM_KHR, m_gbmDevice, NULL);

    if (eglDisplay() == EGL_NO_DISPLAY)
    {
        log(CZError, CZLN, "Failed to get EGL display");
        return false;
    }

    EGLint minor, major;

    if (!eglInitialize(eglDisplay(), &minor, &major))
    {
        log(CZError, CZLN, "Failed to initialize EGL display");
        m_eglDisplay = EGL_NO_DISPLAY;
        return false;
    }

    return true;
}

bool RGLDevice::initEGLDisplayExtensions() noexcept
{
    const char *extensions { eglQueryString(eglDisplay(), EGL_EXTENSIONS) };

    if (!extensions)
    {
        log(CZError, CZLN, "Failed to query EGL display extensions");
        return false;
    }

    auto &exts { m_eglDisplayExtensions };

    exts.KHR_no_config_context = CZStringUtils::CheckExtension(extensions, "EGL_KHR_no_config_context");
    exts.MESA_configless_context = CZStringUtils::CheckExtension(extensions, "EGL_MESA_configless_context");

    if (!exts.KHR_no_config_context && !exts.MESA_configless_context)
    {
        log(CZError, CZLN, "Required EGL extensions EGL_KHR_no_config_context and EGL_MESA_configless_context are not available");
        return false;
    }

    exts.KHR_surfaceless_context = CZStringUtils::CheckExtension(extensions, "EGL_KHR_surfaceless_context");

    if (!exts.KHR_surfaceless_context)
    {
        log(CZError, CZLN, "Required EGL extension KHR_surfaceless_context is not available");
        return false;
    }

    exts.KHR_image_base = CZStringUtils::CheckExtension(extensions, "EGL_KHR_image_base");
    exts.KHR_image = CZStringUtils::CheckExtension(extensions, "EGL_KHR_image");
    exts.EXT_image_dma_buf_import = CZStringUtils::CheckExtension(extensions, "EGL_EXT_image_dma_buf_import");
    exts.EXT_image_dma_buf_import_modifiers = CZStringUtils::CheckExtension(extensions, "EGL_EXT_image_dma_buf_import_modifiers");
    exts.KHR_image_pixmap = CZStringUtils::CheckExtension(extensions, "EGL_KHR_image_pixmap");
    exts.KHR_gl_texture_2D_image = CZStringUtils::CheckExtension(extensions, "EGL_KHR_gl_texture_2D_image");
    exts.KHR_gl_renderbuffer_image = CZStringUtils::CheckExtension(extensions, "EGL_KHR_gl_renderbuffer_image");
    exts.KHR_swap_buffers_with_damage = CZStringUtils::CheckExtension(extensions, "EGL_KHR_swap_buffers_with_damage");

    m_caps.SyncCPU = exts.KHR_fence_sync = CZStringUtils::CheckExtension(extensions, "EGL_KHR_fence_sync");

    if (exts.KHR_fence_sync)
    {
        m_caps.SyncGPU = exts.KHR_wait_sync = CZStringUtils::CheckExtension(extensions, "EGL_KHR_wait_sync");
        m_caps.SyncImport = m_caps.SyncExport = exts.ANDROID_native_fence_sync = CZStringUtils::CheckExtension(extensions, "EGL_ANDROID_native_fence_sync");

        if (m_caps.SyncImport)
        {
            UInt64 soCap { 0 }, tlCap { 0 };
            drmGetCap(drmFd(), DRM_CAP_SYNCOBJ, &soCap);
            drmGetCap(drmFd(), DRM_CAP_SYNCOBJ_TIMELINE, &tlCap);
            m_caps.Timeline = soCap == 1 && tlCap == 1;
        }
    }

    exts.IMG_context_priority = CZStringUtils::CheckExtension(extensions, "EGL_IMG_context_priority");
    return true;
}

bool RGLDevice::initEGLContext() noexcept
{
    if (!eglBindAPI(EGL_OPENGL_ES_API))
    {
        log(CZError, CZLN, "Failed to bind OpenGL ES API");
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

    if (m_eglContext == EGL_NO_CONTEXT)
    {
        log(CZError, CZLN, "Failed to create shared EGL context");
        return 0;
    }

    eglMakeCurrent(eglDisplay(), EGL_NO_SURFACE, EGL_NO_SURFACE, m_eglContext);

    m_skContext = GrDirectContexts::MakeGL(skInterface, CZ::GetSKContextOptions());

    if (!m_skContext)
        log(CZError, CZLN, "Failed to create GL GrDirectContext");

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

    if (exts.KHR_swap_buffers_with_damage)
        procs.eglSwapBuffersWithDamageKHR = (PFNEGLSWAPBUFFERSWITHDAMAGEKHRPROC)eglGetProcAddress("eglSwapBuffersWithDamageKHR");

    if (glExts.OES_EGL_sync)
    {
        if (exts.KHR_fence_sync)
        {
            procs.eglCreateSyncKHR = (PFNEGLCREATESYNCKHRPROC)eglGetProcAddress("eglCreateSyncKHR");
            procs.eglDestroySyncKHR = (PFNEGLDESTROYSYNCKHRPROC)eglGetProcAddress("eglDestroySyncKHR");
            procs.eglClientWaitSyncKHR = (PFNEGLCLIENTWAITSYNCKHRPROC)eglGetProcAddress("eglClientWaitSyncKHR");
            procs.eglGetSyncAttribKHR = (PFNEGLGETSYNCATTRIBKHRPROC)eglGetProcAddress("eglGetSyncAttribKHR");
        }

        if (exts.ANDROID_native_fence_sync)
        {
            procs.eglDupNativeFenceFDANDROID = (PFNEGLDUPNATIVEFENCEFDANDROIDPROC)eglGetProcAddress("eglDupNativeFenceFDANDROID");
        }

        if (exts.KHR_wait_sync)
        {
            procs.eglWaitSyncKHR = (PFNEGLWAITSYNCKHRPROC)eglGetProcAddress("eglWaitSyncKHR");
        }
    }

    if (exts.EXT_image_dma_buf_import_modifiers)
    {
        procs.eglQueryDmaBufFormatsEXT = (PFNEGLQUERYDMABUFFORMATSEXTPROC) eglGetProcAddress("eglQueryDmaBufFormatsEXT");
        procs.eglQueryDmaBufModifiersEXT = (PFNEGLQUERYDMABUFMODIFIERSEXTPROC) eglGetProcAddress("eglQueryDmaBufModifiersEXT");
    }

    return true;
}

bool RGLDevice::initDMAFormats() noexcept
{
    if (!m_eglDisplayExtensions.EXT_image_dma_buf_import)
    {
        log(CZInfo, "EGL DMA Import: false");
        return true;
    }

    EGLint n;
    std::vector<EGLint> formats;

    if (!m_eglDisplayExtensions.EXT_image_dma_buf_import_modifiers)
        goto fallback;

    if (!m_eglDisplayProcs.eglQueryDmaBufFormatsEXT(m_eglDisplay, 0, nullptr, &n) || n <= 0)
        goto fallback;

    formats.resize(n);

    if (!m_eglDisplayProcs.eglQueryDmaBufFormatsEXT(m_eglDisplay, n, formats.data(), &n))
        goto fallback;

    for (auto fmt : formats)
    {
        if (!RDRMFormat::GetInfo(fmt))
            continue;

        EGLBoolean allExternal { true };
        std::vector<EGLuint64KHR> mods;
        std::vector<EGLBoolean> externalOnly;
        n = 0;
        if (!m_eglDisplayProcs.eglQueryDmaBufModifiersEXT(m_eglDisplay, fmt, 0, nullptr, nullptr, &n) || n == 0)
            goto fallback_modifiers;

        mods.resize(n);
        externalOnly.resize(n);

        if (!m_eglDisplayProcs.eglQueryDmaBufModifiersEXT(m_eglDisplay, fmt, n, mods.data(), externalOnly.data(), &n))
            goto fallback_modifiers;

        for (size_t i = 0; i < mods.size(); i++)
        {
            allExternal &= externalOnly[i];
            m_dmaTextureFormats.add(fmt, mods[i]);

            if (!externalOnly[i])
                m_dmaRenderFormats.add(fmt, mods[i]);
        }

    fallback_modifiers:
        if (n == 0)
        {
            m_dmaRenderFormats.add(fmt, DRM_FORMAT_MOD_LINEAR);
            m_dmaTextureFormats.add(fmt, DRM_FORMAT_MOD_LINEAR);
        }

        m_dmaTextureFormats.add(fmt, DRM_FORMAT_MOD_INVALID);

        if (n == 0 || !allExternal)
            m_dmaRenderFormats.add(fmt, DRM_FORMAT_MOD_INVALID);
    }

    return true;
fallback:
    log(CZInfo, "EGL DMA Import: Without Modifiers");
    m_dmaRenderFormats.add(DRM_FORMAT_ARGB8888, DRM_FORMAT_MOD_INVALID);
    m_dmaRenderFormats.add(DRM_FORMAT_XRGB8888, DRM_FORMAT_MOD_INVALID);
    m_dmaRenderFormats.add(DRM_FORMAT_ARGB8888, DRM_FORMAT_MOD_LINEAR);
    m_dmaRenderFormats.add(DRM_FORMAT_XRGB8888, DRM_FORMAT_MOD_LINEAR);
    m_dmaTextureFormats = m_dmaRenderFormats;
    return true;
}

bool RGLDevice::initFormats() noexcept
{
    m_textureFormats = m_dmaTextureFormats;
    m_renderFormats = m_dmaRenderFormats;

    m_textureFormats.add(DRM_FORMAT_ABGR8888, DRM_FORMAT_MOD_INVALID);
    m_textureFormats.add(DRM_FORMAT_BGR888, DRM_FORMAT_MOD_INVALID);

    if (glExtensions().EXT_texture_format_BGRA8888)
        m_textureFormats.add(DRM_FORMAT_ARGB8888, DRM_FORMAT_MOD_INVALID);

    m_renderFormats.add(DRM_FORMAT_ABGR8888, DRM_FORMAT_MOD_INVALID);
    return true;
}

bool RGLDevice::initPainter() noexcept
{
    m_threadData = RGLContextDataManager::MakeInternal([](RGLDevice *device) -> RGLContextData *
    {
        return new ThreadData(device);
    });

    return true;
}

std::shared_ptr<RPainter> RGLDevice::makePainter(std::shared_ptr<RSurface> surface) noexcept
{
    assert(surface);
    return std::shared_ptr<RPainter>(new RGLPainter(surface, this));
}

RGLDevice::ThreadData::ThreadData(RGLDevice *device) noexcept
{
    CZ_UNUSED(device)
}
