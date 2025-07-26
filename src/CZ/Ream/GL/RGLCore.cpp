#include <CZ/Ream/GL/RGLStrings.h>
#include <CZ/Ream/GL/RGLCore.h>
#include <CZ/Ream/GL/RGLDevice.h>

#include <CZ/Ream/WL/RWLPlatformHandle.h>
#include <CZ/Ream/DRM/RDRMPlatformHandle.h>

#include <CZ/Ream/RLog.h>

#include <CZ/Utils/CZStringUtils.h>

#include <drm/drm_fourcc.h>

#include <EGL/egl.h>
#include <fcntl.h>
#include <gbm.h>

using namespace CZ;

static CZLogger logEGL;

static void EGLLog(EGLenum error, const char *command, EGLint type, EGLLabelKHR thread, EGLLabelKHR obj, const char *msg)
{
    CZ_UNUSED(thread);
    CZ_UNUSED(obj);

    CZLogLevel level { CZDebug };

    switch (type)
    {
    case EGL_DEBUG_MSG_CRITICAL_KHR:
        level = CZFatal;
        break;
    case EGL_DEBUG_MSG_ERROR_KHR:
        level = CZError;
        break;
    case EGL_DEBUG_MSG_WARN_KHR:
        level = CZWarning;
        break;
    case EGL_DEBUG_MSG_INFO_KHR:
        level = CZInfo;
        break;
    default:
        break;
    }

    if (command && msg)
        logEGL(level, "command: {}, error: {}, message: {}", command, RGLStrings::EGLError(error), msg);
}

bool RGLCore::initClientEGLExtensions() noexcept
{
    if (eglBindAPI(EGL_OPENGL_ES_API) == EGL_FALSE)
    {
        RLog(CZError, CZLN, "Failed to bind OpenGL ES API");
        return false;
    }

    const char *extensions = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);

    if (!extensions)
    {
        if (eglGetError() == EGL_BAD_DISPLAY)
            RLog(CZError, CZLN, "EGL_EXT_client_extensions not supported. Required to query its existence without a display");
        else
            RLog(CZError, CZLN, "Failed to query EGL client extensions");

        return false;
    }

    RLog(CZDebug, "EGL Client Extensions: {}", extensions);

    auto &exts { m_clientEGLExtensions };
    auto &procs { m_clientEGLProcs };

    exts.EXT_platform_base = CZStringUtils::CheckExtension(extensions, "EGL_EXT_platform_base");

    if (!exts.EXT_platform_base)
    {
        RLog(CZError, CZLN, "EGL_EXT_platform_base not supported");
        return false;
    }

    procs.eglGetPlatformDisplayEXT = (PFNEGLGETPLATFORMDISPLAYEXTPROC) eglGetProcAddress("eglGetPlatformDisplayEXT");
    exts.KHR_platform_gbm = CZStringUtils::CheckExtension(extensions, "EGL_KHR_platform_gbm");
    exts.MESA_platform_gbm = CZStringUtils::CheckExtension(extensions, "EGL_MESA_platform_gbm");

    if (m_options.platformHandle->platform() == RPlatform::DRM && !exts.KHR_platform_gbm && !exts.MESA_platform_gbm)
    {
        RLog(CZError, CZLN, "EGL_KHR_platform_gbm not supported and is required by the DRM platform");
        return false;
    }

    exts.KHR_platform_wayland = CZStringUtils::CheckExtension(extensions, "EGL_KHR_platform_wayland");

    if (m_options.platformHandle->platform() == RPlatform::Wayland && !exts.KHR_platform_wayland)
    {
        RLog(CZError, CZLN, "EGL_KHR_platform_wayland not supported");
        return false;
    }

    exts.EXT_device_base = CZStringUtils::CheckExtension(extensions, "EGL_EXT_device_base");
    exts.EXT_device_query = CZStringUtils::CheckExtension(extensions, "EGL_EXT_device_query");

    if (exts.EXT_device_base || exts.EXT_device_query)
    {
        exts.EXT_device_query = true;
        procs.eglQueryDeviceStringEXT = (PFNEGLQUERYDEVICESTRINGEXTPROC) eglGetProcAddress("eglQueryDeviceStringEXT");
        procs.eglQueryDisplayAttribEXT = (PFNEGLQUERYDISPLAYATTRIBEXTPROC) eglGetProcAddress("eglQueryDisplayAttribEXT");
    }

    exts.KHR_debug = CZStringUtils::CheckExtension(extensions, "EGL_KHR_debug");

    if (exts.KHR_debug)
    {
        procs.eglDebugMessageControlKHR = (PFNEGLDEBUGMESSAGECONTROLKHRPROC) eglGetProcAddress("eglDebugMessageControlKHR");

        logEGL = CZLogger("Ream", "CZ_REAM_EGL_LOG_LEVEL");
        logEGL = logEGL.newWithContext("EGL");

        const EGLAttrib debugAttribs[]
        {
            EGL_DEBUG_MSG_CRITICAL_KHR, logEGL.level() > 0 ? EGL_TRUE : EGL_FALSE,
            EGL_DEBUG_MSG_ERROR_KHR,    logEGL.level() > 1 ? EGL_TRUE : EGL_FALSE,
            EGL_DEBUG_MSG_WARN_KHR,     logEGL.level() > 2 ? EGL_TRUE : EGL_FALSE,
            EGL_DEBUG_MSG_INFO_KHR,     logEGL.level() > 3 ? EGL_TRUE : EGL_FALSE,
            EGL_NONE,
        };

        procs.eglDebugMessageControlKHR(EGLLog, debugAttribs);
    }
    return true;
}

bool RGLCore::initDevices() noexcept
{
    if (platform() == RPlatform::Wayland)
    {
        m_mainDevice = RGLDevice::Make(*this, -1, nullptr);

        if (m_mainDevice)
            m_devices.emplace_back(m_mainDevice);
    }
    else // DRM
    {
        for (auto &handle : options().platformHandle->asDRM()->fds())
        {
            auto *dev { RGLDevice::Make(*this, handle.fd, handle.userData) };

            if (dev)
            {
                m_mainDevice = dev; // TODO
                m_devices.emplace_back(dev);
            }
        }
    }

    return !m_devices.empty();
}

void RGLCore::unitDevices() noexcept
{
    while (!m_devices.empty())
    {
        delete devices().back();
        m_devices.pop_back();
    }
}

RGLCore::RGLCore(const Options &options) noexcept : RCore(options)
{
    m_options.graphicsAPI = RGraphicsAPI::GL;
}

RGLCore::~RGLCore()
{
    unitDevices();
}
