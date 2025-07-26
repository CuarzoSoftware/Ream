#include <CZ/Ream/GL/RGLMakeCurrent.h>
#include <CZ/Ream/EGL/REGLImage.h>
#include <CZ/Ream/GL/RGLContext.h>
#include <CZ/Ream/GL/RGLCore.h>
#include <CZ/Ream/GL/RGLDevice.h>
#include <CZ/Ream/RDMABufferInfo.h>
#include <CZ/Ream/RLog.h>
#include <drm_fourcc.h>

using namespace CZ;

const static EGLint EGLPlaneFd[]
{
    EGL_DMA_BUF_PLANE0_FD_EXT,
    EGL_DMA_BUF_PLANE1_FD_EXT,
    EGL_DMA_BUF_PLANE2_FD_EXT,
    EGL_DMA_BUF_PLANE3_FD_EXT,
};

const static EGLint EGLPlaneOffset[]
{
    EGL_DMA_BUF_PLANE0_OFFSET_EXT,
    EGL_DMA_BUF_PLANE1_OFFSET_EXT,
    EGL_DMA_BUF_PLANE2_OFFSET_EXT,
    EGL_DMA_BUF_PLANE3_OFFSET_EXT,
};

const static EGLint EGLPlanePitch[]
{
    EGL_DMA_BUF_PLANE0_PITCH_EXT,
    EGL_DMA_BUF_PLANE1_PITCH_EXT,
    EGL_DMA_BUF_PLANE2_PITCH_EXT,
    EGL_DMA_BUF_PLANE3_PITCH_EXT,
};

const static EGLint EGLPlaneModLo[]
{
    EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT,
    EGL_DMA_BUF_PLANE1_MODIFIER_LO_EXT,
    EGL_DMA_BUF_PLANE2_MODIFIER_LO_EXT,
    EGL_DMA_BUF_PLANE3_MODIFIER_LO_EXT,
};

const static EGLint EGLPlaneModHi[]
{
    EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT,
    EGL_DMA_BUF_PLANE1_MODIFIER_HI_EXT,
    EGL_DMA_BUF_PLANE2_MODIFIER_HI_EXT,
    EGL_DMA_BUF_PLANE3_MODIFIER_HI_EXT,
};

std::shared_ptr<REGLImage> REGLImage::MakeFromDMA(const RDMABufferInfo &info, RGLDevice *device) noexcept
{
    auto core { RCore::Get() };

    if (!core)
    {
        RLog(CZError, CZLN, "Missing RCore");
        return {};
    }

    auto glCore { core->asGL() };

    if (!glCore)
    {
        RLog(CZError, CZLN, "The RCore must be an RGLCore");
        return {};
    }

    if (!device)
        device = glCore->mainDevice();

    GLenum target { GL_TEXTURE_2D };

    if (!device->dmaTextureFormats().has(info.format, info.modifier))
    {
        device->log(CZError, CZLN, "Unsupported format {} - {}", RDRMFormat::FormatName(info.format), RDRMFormat::ModifierName(info.modifier));
        return {};
    }

    if (!device->dmaRenderFormats().has(info.format, info.modifier))
        target = GL_TEXTURE_EXTERNAL_OES;

    auto current { RGLMakeCurrent::FromDevice(device, false) };

    size_t atti { 0 };
    EGLint attribs[50];
    attribs[atti++] = EGL_WIDTH;
    attribs[atti++] = info.width;
    attribs[atti++] = EGL_HEIGHT;
    attribs[atti++] = info.height;
    attribs[atti++] = EGL_LINUX_DRM_FOURCC_EXT;
    attribs[atti++] = info.format;

    for (int i = 0; i < info.planeCount; i++)
    {
        attribs[atti++] = EGLPlaneFd[i];
        attribs[atti++] = info.fd[i];
        attribs[atti++] = EGLPlaneOffset[i];
        attribs[atti++] = info.offset[i];
        attribs[atti++] = EGLPlanePitch[i];
        attribs[atti++] = info.stride[i];

        if (info.modifier != DRM_FORMAT_MOD_INVALID)
        {
            attribs[atti++] = EGLPlaneModLo[i];
            attribs[atti++] = info.modifier & 0xFFFFFFFF;
            attribs[atti++] = EGLPlaneModHi[i];
            attribs[atti++] = info.modifier >> 32;
        }
    }
    attribs[atti++] = EGL_IMAGE_PRESERVED_KHR;
    attribs[atti++] = EGL_TRUE;
    attribs[atti++] = EGL_NONE;

    EGLImage image { device->eglDisplayProcs().eglCreateImageKHR(
        device->eglDisplay(),
        EGL_NO_CONTEXT,
        EGL_LINUX_DMA_BUF_EXT,
        NULL, attribs) };

    if (image == EGL_NO_IMAGE)
    {
        device->log(CZError, CZLN, "Failed to create EGLImage");
        return {};
    }

    return std::shared_ptr<REGLImage>(new REGLImage(glCore, device, image, target));
}

RGLTexture REGLImage::texture() const noexcept
{
    if (m_tex.id)
        return m_tex;

    auto current { RGLMakeCurrent::FromDevice(m_device, false) };
    glGenTextures(1, &m_tex.id);

    // TODO: Check EGL formats
    glBindTexture(m_tex.target, m_tex.id);
    m_device->eglDisplayProcs().glEGLImageTargetTexture2DOES(m_tex.target, m_eglImage);
    glBindTexture(m_tex.target, 0);
    return m_tex;
}

GLuint REGLImage::fb() const noexcept
{
    auto *data { static_cast<CtxData*>(m_ctxDataManager->getData(m_device)) };

    if (data->fb)
        return data->fb;

    auto current { RGLMakeCurrent::FromDevice(m_device, false) };

    if (!data->rbo)
    {
        glGenRenderbuffers(1, &data->rbo);
        glBindRenderbuffer(GL_RENDERBUFFER, data->rbo);
        m_device->eglDisplayProcs().glEGLImageTargetRenderbufferStorageOES(GL_RENDERBUFFER, m_eglImage);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
    }

    glGenFramebuffers(1, &data->fb);
    glBindFramebuffer(GL_FRAMEBUFFER, data->fb);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, data->rbo);
    const GLenum status { glCheckFramebufferStatus(GL_FRAMEBUFFER) };
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        m_device->log(CZError, CZLN, "Failed to create GL fb");
        glDeleteFramebuffers(1, &data->fb);
        data->fb = 0;
    }
    return data->fb;
}

REGLImage::~REGLImage() noexcept
{
    m_ctxDataManager->freeData(m_device);
    auto current { RGLMakeCurrent::FromDevice(m_device, false) };

    if (m_tex.id)
    {
        glBindTexture(m_tex.target, 0);
        glDeleteTextures(1, &m_tex.id);
    }

    eglDestroyImage(m_device->eglDisplay(), m_eglImage);
}

REGLImage::REGLImage(std::shared_ptr<RGLCore> core, RGLDevice *device, EGLImage image, GLenum target) noexcept :
    m_core(core),
    m_eglImage(image),
    m_device(device)
{
    m_tex.target = target;
    assert(image != EGL_NO_IMAGE);
    assert(device);
    assert(core);

    m_ctxDataManager = RGLContextDataManager::Make([](RGLDevice *device) -> RGLContextData*
    {
        return new CtxData(device);
    });
}

REGLImage::CtxData::~CtxData() noexcept
{
    if (fb)
        glDeleteFramebuffers(1, &fb);

    if (rbo)
        glDeleteRenderbuffers(1, &rbo);
}
