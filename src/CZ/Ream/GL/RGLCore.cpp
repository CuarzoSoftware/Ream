#include "CZ/Ream/GL/EGL/REGLString.h"
#include <CZ/Ream/GL/RGLCore.h>
#include <CZ/Ream/RLog.h>

#include <CZ/Utils/CZStringUtils.h>

#include <EGL/egl.h>

using namespace CZ;

bool RGLCore::init() noexcept
{
    if (initInstanceEGLExtensions() &&
        initInstanceEGLProcs())
        return true;

    return false;
}

bool RGLCore::initInstanceEGLExtensions() noexcept
{
    if (eglBindAPI(EGL_OPENGL_ES_API) == EGL_FALSE)
    {
        RError(RLINE, "Failed to bind OpenGL ES API.");
        return false;
    }

    const char *extensions = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);

    if (!extensions)
    {
        RError(RLINE, "Failed to query instance EGL extensions.");
        return false;
    }

    m_instanceEGLExtensions.EXT_platform_base = CZStringUtils::CheckExtension(extensions, "EGL_EXT_platform_base");

    if (!m_instanceEGLExtensions.EXT_platform_base)
    {
        RError(RLINE, "EGL_EXT_platform_base not supported.");
        return false;
    }

    m_instanceEGLExtensions.KHR_platform_gbm = CZStringUtils::CheckExtension(extensions, "EGL_KHR_platform_gbm");
    m_instanceEGLExtensions.MESA_platform_gbm = CZStringUtils::CheckExtension(extensions, "EGL_MESA_platform_gbm");

    if (!m_instanceEGLExtensions.KHR_platform_gbm && !m_instanceEGLExtensions.MESA_platform_gbm)
    {
        RError(RLINE, "EGL_KHR_platform_gbm not supported.");
        return false;
    }

    m_instanceEGLExtensions.EXT_platform_device = CZStringUtils::CheckExtension(extensions, "EGL_EXT_platform_device");
    m_instanceEGLExtensions.KHR_display_reference = CZStringUtils::CheckExtension(extensions, "EGL_KHR_display_reference");
    m_instanceEGLExtensions.EXT_device_base = CZStringUtils::CheckExtension(extensions, "EGL_EXT_device_base");
    m_instanceEGLExtensions.EXT_device_enumeration = CZStringUtils::CheckExtension(extensions, "EGL_EXT_device_enumeration");
    m_instanceEGLExtensions.EXT_device_query = CZStringUtils::CheckExtension(extensions, "EGL_EXT_device_query");
    m_instanceEGLExtensions.KHR_debug = CZStringUtils::CheckExtension(extensions, "EGL_KHR_debug");
    return true;
}

static void EGLLog(EGLenum error, const char *command, EGLint type, EGLLabelKHR thread, EGLLabelKHR obj, const char *msg)
{
    CZ_UNUSED(thread);
    CZ_UNUSED(obj);

    static const char *format = "[EGL] command: %s, error: %s (0x%x), message: \"%s\".";

    switch (type)
    {
    case EGL_DEBUG_MSG_CRITICAL_KHR:
        RFatal(format, command, REGLString::Error(error), error, msg);
        break;
    case EGL_DEBUG_MSG_ERROR_KHR:
        RError(format, command, REGLString::Error(error), error, msg);
        break;
    case EGL_DEBUG_MSG_WARN_KHR:
        RWarning(format, command, REGLString::Error(error), error, msg);
        break;
    case EGL_DEBUG_MSG_INFO_KHR:
        RDebug(format, command, REGLString::Error(error), error, msg);
        break;
    default:
        RDebug(format, command, REGLString::Error(error), error, msg);
        break;
    }
}

bool RGLCore::initInstanceEGLProcs() noexcept
{
    m_instanceEGLProcs.eglGetPlatformDisplayEXT = (PFNEGLGETPLATFORMDISPLAYEXTPROC) eglGetProcAddress("eglGetPlatformDisplayEXT");

    if (m_instanceEGLExtensions.EXT_device_base || m_instanceEGLExtensions.EXT_device_enumeration)
        m_instanceEGLProcs.eglQueryDevicesEXT = (PFNEGLQUERYDEVICESEXTPROC) eglGetProcAddress("eglQueryDevicesEXT");

    if (m_instanceEGLExtensions.EXT_device_base || m_instanceEGLExtensions.EXT_device_query)
    {
        m_instanceEGLExtensions.EXT_device_query = 1;
        m_instanceEGLProcs.eglQueryDeviceStringEXT = (PFNEGLQUERYDEVICESTRINGEXTPROC) eglGetProcAddress("eglQueryDeviceStringEXT");
        m_instanceEGLProcs.eglQueryDisplayAttribEXT = (PFNEGLQUERYDISPLAYATTRIBEXTPROC) eglGetProcAddress("eglQueryDisplayAttribEXT");
    }

    if (m_instanceEGLExtensions.KHR_debug)
    {
        m_instanceEGLProcs.eglDebugMessageControlKHR = (PFNEGLDEBUGMESSAGECONTROLKHRPROC) eglGetProcAddress("eglDebugMessageControlKHR");

        const int level { REGLLogLevel() };

        const EGLAttrib debugAttribs[]
        {
            EGL_DEBUG_MSG_CRITICAL_KHR, level > 0 ? EGL_TRUE : EGL_FALSE,
            EGL_DEBUG_MSG_ERROR_KHR,    level > 1 ? EGL_TRUE : EGL_FALSE,
            EGL_DEBUG_MSG_WARN_KHR,     level > 2 ? EGL_TRUE : EGL_FALSE,
            EGL_DEBUG_MSG_INFO_KHR,     level > 3 ? EGL_TRUE : EGL_FALSE,
            EGL_NONE,
        };

        m_instanceEGLProcs.eglDebugMessageControlKHR(EGLLog, debugAttribs);
    }

    return true;
}

RGLCore::RGLCore(const Options &options) noexcept : RCore(options)
{
    m_options.graphicsAPI = RGraphicsAPI::GL;
}
