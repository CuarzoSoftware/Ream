#include <CZ/Ream/GL/RGLStrings.h>
#include <CZ/Ream/GL/RGLCore.h>
#include <CZ/Ream/GL/RGLDevice.h>

#include <CZ/Ream/WL/RWLPlatformHandle.h>

#include <CZ/Ream/RLog.h>

#include <CZ/Utils/CZStringUtils.h>

#include <drm/drm_fourcc.h>

#include <EGL/egl.h>
#include <fcntl.h>
#include <gbm.h>

using namespace CZ;

static void EGLLog(EGLenum error, const char *command, EGLint type, EGLLabelKHR thread, EGLLabelKHR obj, const char *msg)
{
    CZ_UNUSED(thread);
    CZ_UNUSED(obj);

    static const char *format = "[EGL] command: %s, error: %s (0x%x), message: \"%s\".";

    switch (type)
    {
    case EGL_DEBUG_MSG_CRITICAL_KHR:
        RFatal(format, command, RGLStrings::EGLError(error), error, msg);
        break;
    case EGL_DEBUG_MSG_ERROR_KHR:
        RError(format, command, RGLStrings::EGLError(error), error, msg);
        break;
    case EGL_DEBUG_MSG_WARN_KHR:
        RWarning(format, command, RGLStrings::EGLError(error), error, msg);
        break;
    case EGL_DEBUG_MSG_INFO_KHR:
        RDebug(format, command, RGLStrings::EGLError(error), error, msg);
        break;
    default:
        RDebug(format, command, RGLStrings::EGLError(error), error, msg);
        break;
    }
}

bool RGLCore::initClientEGLExtensions() noexcept
{
    if (eglBindAPI(EGL_OPENGL_ES_API) == EGL_FALSE)
    {
        RError(CZLN, "Failed to bind OpenGL ES API.");
        return false;
    }

    const char *extensions = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);

    if (!extensions)
    {
        if (eglGetError() == EGL_BAD_DISPLAY)
            RError(CZLN, "EGL_EXT_client_extensions not supported. Required to query its existence without a display.");
        else
            RError(CZLN, "Failed to query EGL client extensions");

        return false;
    }

    auto &exts { m_clientEGLExtensions };
    auto &procs { m_clientEGLProcs };

    exts.EXT_platform_base = CZStringUtils::CheckExtension(extensions, "EGL_EXT_platform_base");

    if (!exts.EXT_platform_base)
    {
        RError(CZLN, "EGL_EXT_platform_base not supported.");
        return false;
    }

    procs.eglGetPlatformDisplayEXT = (PFNEGLGETPLATFORMDISPLAYEXTPROC) eglGetProcAddress("eglGetPlatformDisplayEXT");
    exts.KHR_platform_gbm = CZStringUtils::CheckExtension(extensions, "EGL_KHR_platform_gbm");
    exts.MESA_platform_gbm = CZStringUtils::CheckExtension(extensions, "EGL_MESA_platform_gbm");

    if (m_options.platformHandle->platform() == RPlatform::DRM && !exts.KHR_platform_gbm && !exts.MESA_platform_gbm)
    {
        RError(CZLN, "EGL_KHR_platform_gbm not supported and is required by the DRM platform.");
        return false;
    }

    exts.KHR_platform_wayland = CZStringUtils::CheckExtension(extensions, "EGL_KHR_platform_wayland");

    if (m_options.platformHandle->platform() == RPlatform::Wayland && !exts.KHR_platform_wayland)
    {
        RError(CZLN, "EGL_KHR_platform_wayland not supported.");
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

        const int level { REGLLogLevel() };

        const EGLAttrib debugAttribs[]
        {
            EGL_DEBUG_MSG_CRITICAL_KHR, level > 0 ? EGL_TRUE : EGL_FALSE,
            EGL_DEBUG_MSG_ERROR_KHR,    level > 1 ? EGL_TRUE : EGL_FALSE,
            EGL_DEBUG_MSG_WARN_KHR,     level > 2 ? EGL_TRUE : EGL_FALSE,
            EGL_DEBUG_MSG_INFO_KHR,     level > 3 ? EGL_TRUE : EGL_FALSE,
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
        m_mainDevice = RGLDevice::Make(*this, -1);

        if (m_mainDevice)
            m_devices.emplace_back(m_mainDevice);
    }

    return !m_devices.empty();
}

void RGLCore::unitDevices() noexcept
{
    while (!devices().empty())
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
