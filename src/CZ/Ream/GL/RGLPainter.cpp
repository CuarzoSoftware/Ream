#include <CZ/Ream/GL/RGLImageWrap.h>
#include <CZ/Ream/RLog.h>
#include <CZ/Ream/GL/RGLPainter.h>
#include <CZ/Ream/GL/RGLDevice.h>
#include <CZ/Ream/GL/RGLMakeCurrent.h>
#include <CZ/Ream/GL/RGLImage.h>
#include <CZ/Ream/GL/RGLProgram.h>
#include <CZ/Ream/RSurface.h>
#include <CZ/Ream/RSync.h>
#include <CZ/skia/core/SkMatrix.h>

using namespace CZ;

void RGLPainter::calcPosProj(RSurface *surface, bool flipY, SkScalar *outMat) const noexcept
{
    SkMatrix mat { SkMatrix::I() };
    mat.preScale(
        2.f / SkScalar(surface->size().width()),
        2.f / SkScalar(surface->size().height()));
    mat.postTranslate(-1.0f, -1.f);
    if (flipY)
        mat.postScale(1.f, -1.f);
    mat.get9(outMat);
}

void RGLPainter::calcImageProj(const RDrawImageInfo &info, SkScalar *outMat) const noexcept
{
    SkMatrix mat = SkMatrix::I();
    mat.postTranslate(-info.dst.x(), -info.dst.y());
    Float32 sx = info.src.width() / SkScalar(info.dst.width());
    Float32 sy = info.src.height() / SkScalar(info.dst.height());
    mat.postScale(sx, sy);
    mat.postTranslate(info.src.x(), info.src.y());
    mat.postScale(
        info.srcScale / SkScalar(info.image->size().width()),
        info.srcScale / SkScalar(info.image->size().height()));
    mat.get9(outMat);
}

void RGLPainter::setScissors(RSurface *surface, bool flipY, const SkRegion &region) const noexcept
{
    glEnable(GL_SCISSOR_TEST);
    const SkIRect bounds { region.getBounds().makeOffset(-surface->pos()) };

    if (flipY)
    {
        glScissor(
            bounds.x() * surface->scale(),
            (surface->size().height() - bounds.bottom()) * surface->scale(),
            bounds.width() * surface->scale(),
            bounds.height() * surface->scale());
    }
    else
        glScissor(
            bounds.x() * surface->scale(),
            bounds.y() * surface->scale(),
            bounds.width() * surface->scale(),
            bounds.height() * surface->scale());
}

void RGLPainter::bindTexture(RGLTexture tex, GLuint uniform, const RDrawImageInfo &info, GLuint slot) const noexcept
{
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(tex.target, tex.id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, info.minFilter == RImageFilter::Linear ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, info.minFilter == RImageFilter::Linear ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, RImageWrapToGL(info.wrapS));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, RImageWrapToGL(info.wrapT));
    glUniform1i(uniform, slot);
}

bool RGLPainter::drawImage(const RDrawImageInfo &imageInfo, const SkRegion *clip, const RDrawImageInfo *maskInfo) noexcept
{
    auto surface { m_surface.lock() };

    if (!surface || !surface->image())
    {
        RLog(CZError, CZLN, "Invalid RSurface");
        return false;
    }

    auto image { imageInfo.image };

    if (!image)
    {
        RLog(CZError, CZLN, "Missing source RImage");
        return false;
    }

    const SkRegion region { calcDrawImageRegion(surface.get(), imageInfo, clip, maskInfo) };

    if (region.isEmpty())
        return true; // Nothing to draw

    std::shared_ptr<RImage> mask;
    RGLTexture maskTex {};

    if (maskInfo)
    {
        mask = maskInfo->image;

        if (!mask)
        {
            RLog(CZError, CZLN, "Invalid mask");
            return false;
        }

        // Disable mask if it only has alpha = 1.f
        if (mask->alphaType() == SkAlphaType::kOpaque_SkAlphaType)
        {
            mask.reset();
            goto skipMask;
        }

        maskTex = maskInfo->image->asGL()->texture(device());

        if (maskTex.id == 0)
        {
            RLog(CZError, CZLN, "Failed to get GL texture from mask");
            return false;
        }

        if (mask->writeSync())
            mask->writeSync()->gpuWait(device());
    }

skipMask:

    if (image->writeSync())
        image->writeSync()->gpuWait(device());

    auto fb { surface->image()->asGL()->glFb(device()) };

    if (!fb.has_value())
    {
        device()->log(CZError, CZLN, "Failed to get GL framebuffer from RGLImage");
        return false;
    }

    auto tex { image->asGL()->texture(device()) };

    if (tex.id == 0)
    {
        device()->log(CZError, CZLN, "Failed to get GL texture from RGLImage");
        return false;
    }

    auto current { RGLMakeCurrent::FromDevice(device(), true) };
    const auto features { calcFeatures(image, mask) };
    auto prog { RGLProgram::GetOrMake(this, features) };

    if (!prog)
    {
        device()->log(CZError, CZLN, "Failed to create a GL program with the required features");
        return false;
    }

    // Color
    SkColor4f colorF { m_state.factor };

    if (image->alphaType() == kPremul_SkAlphaType)
    {
        colorF.fA *= m_state.opacity;
        colorF.fR *= colorF.fA;
        colorF.fG *= colorF.fA;
        colorF.fB *= colorF.fA;
    }
    else
    {
        colorF.fA *= m_state.opacity;
    }

    if (image->alphaType() == kOpaque_SkAlphaType && m_state.blendMode == RBlendMode::DstIn)
    {
        if (colorF.fA >= 1.f)
        {
            device()->log(CZTrace, "Skipping drawImage(DstIn, alpha = 1.f, Opaque): multiplying dst by 1.f is a noop");
            return true;
        }
        else
        {
            /* Avoid sampling if alpha is constant */
            save();
            reset();
            setBlendMode(RBlendMode::DstIn);
            setOpacity(colorF.fA);
            setColor(SK_ColorWHITE);
            setOptions(ColorIsPremult);
            const bool ret { drawColor(region) };
            restore();
            return ret;
        }
    }

    prog->bind();
    glBindFramebuffer(GL_FRAMEBUFFER, fb.value());
    bindTexture(tex, prog->loc().image, imageInfo, 0);

    if (features.has(RGLShader::HasMask))
        bindTexture(maskTex, prog->loc().mask, *maskInfo, 1);

    if (features.has(RGLShader::HasFactorA))
        glUniform1f(prog->loc().factorA, colorF.fA);

    if (features.has(RGLShader::HasFactorR))
        glUniform1f(prog->loc().factorR, colorF.fR);

    if (features.has(RGLShader::HasFactorG))
        glUniform1f(prog->loc().factorG, colorF.fG);

    if (features.has(RGLShader::HasFactorB))
        glUniform1f(prog->loc().factorB, colorF.fB);

    // posProj
    SkScalar mat[9];
    calcPosProj(surface.get(), fb.value() == 0, mat);
    glUniformMatrix3fv(prog->loc().posProj, 1, GL_FALSE, mat);

    // imageProj
    calcImageProj(imageInfo, mat);
    glUniformMatrix3fv(prog->loc().imageProj, 1, GL_FALSE, mat);

    if (mask)
    {
        // maskProj
        calcImageProj(*maskInfo, mat);
        glUniformMatrix3fv(prog->loc().maskProj, 1, GL_FALSE, mat);
    }

    if (m_state.blendMode == RBlendMode::Src)
    {
        /* Even in this mode, we need to enable blending and convert the image to premultiplied */
        if (image->alphaType() == kUnpremul_SkAlphaType)
        {
            glEnable(GL_BLEND);
            glBlendEquation(GL_FUNC_ADD);
            glBlendFuncSeparate(GL_SRC_ALPHA, GL_ZERO, GL_ONE, GL_ZERO);
        }
        else
            glDisable(GL_BLEND);
    }
    else if (m_state.blendMode == RBlendMode::SrcOver)
    {
        if (image->alphaType() == kOpaque_SkAlphaType)
        {
            if (colorF.fA >= 1.f)
                glDisable(GL_BLEND);
            else
            {
                glEnable(GL_BLEND);
                glBlendEquation(GL_FUNC_ADD);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            }
        }
        else if (image->alphaType() == kUnpremul_SkAlphaType)
        {
            glEnable(GL_BLEND);
            glBlendEquation(GL_FUNC_ADD);
            glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        }
        else // Premult
        {
            glEnable(GL_BLEND);
            glBlendEquation(GL_FUNC_ADD);
            glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        }
    }
    else // DstIn
    {
        /* We only care about A so the alphaType() doesn't matter.
         * This function exits earlier if the alpha type is Opaque
         * and alpha = 1.f */
        glEnable(GL_BLEND);
        glBlendEquation(GL_FUNC_ADD);
        glBlendFunc(GL_ZERO, GL_SRC_ALPHA);
    }

    std::vector<GLfloat> vbo { genVBO(region) };
    glEnableVertexAttribArray(prog->loc().pos);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, vbo.data());
    const auto size { surface->image()->size() };
    glViewport(0, 0, size.width(), size.height());
    setScissors(surface.get(), fb == 0, region);
    glBlendEquation(GL_FUNC_ADD);
    glDrawArrays(GL_TRIANGLES, 0, region.computeRegionComplexity() * 6);
    glDisableVertexAttribArray(prog->loc().pos);
    return true;
}

std::vector<GLfloat> RGLPainter::genVBO(const SkRegion &region) const noexcept
{
    std::vector<GLfloat> vbo;
    vbo.resize(region.computeRegionComplexity() * 12);

    SkRegion::Iterator it { region };
    size_t i { 0 };
    while (!it.done())
    {
        // Triangle 1

        // Bottom left
        vbo[i++] = it.rect().fLeft;
        vbo[i++] = it.rect().fBottom;

        // Bottom right
        vbo[i++] = it.rect().fRight;
        vbo[i++] = it.rect().fBottom;

        // Top left
        vbo[i++] = it.rect().fLeft;
        vbo[i++] = it.rect().fTop;

        // Triangle 2

        // Top left
        vbo[i++] = it.rect().fLeft;
        vbo[i++] = it.rect().fTop;

        // Bottom right
        vbo[i++] = it.rect().fRight;
        vbo[i++] = it.rect().fBottom;

        // Top Right
        vbo[i++] = it.rect().fRight;
        vbo[i++] = it.rect().fTop;

        it.next();
    }

    return vbo;
}

SkRegion RGLPainter::calcDrawImageRegion(RSurface *surface, const RDrawImageInfo &imageInfo, const SkRegion *clip, const RDrawImageInfo *maskInfo) const noexcept
{
    SkRegion region { SkIRect::MakePtSize(surface->pos(), surface->size()) };
    region.op(imageInfo.dst, SkRegion::Op::kIntersect_Op);

    if (maskInfo)
        region.op(maskInfo->dst, SkRegion::Op::kIntersect_Op);

    if (clip)
        region.op(*clip, SkRegion::Op::kIntersect_Op);

    return region;
}


bool RGLPainter::drawColor(const SkRegion &userRegion) noexcept
{
    auto surface { m_surface.lock() };

    if (!surface || !surface->image())
        return false;

    const SkIRect clipRect { SkIRect::MakePtSize(surface->pos(), surface->size()) };

    SkRegion region;

    if (!region.op(clipRect, userRegion, SkRegion::kIntersect_Op))
        return true; // Empty

    auto fb { surface->image()->asGL()->glFb(device()) };

    if (!fb.has_value())
    {
        device()->log(CZError, CZLN, "Failed to get GL framebuffer from RGLImage");
        return false;
    }

    auto current { RGLMakeCurrent::FromDevice(device(), true) };
    auto features { calcFeatures(nullptr, nullptr) };
    auto prog { RGLProgram::GetOrMake(this, features) };

    if (!prog)
    {
        device()->log(CZError, CZLN, "Failed to create a GL program with the required features");
        return false;
    }

    // Color
    SkColor4f colorF { SkColor4f::FromColor(color()) };

    if (m_state.options.has(ColorIsPremult))
    {
        const SkScalar alpha { m_state.opacity * m_state.factor.fA };
        colorF.fR *= alpha * m_state.factor.fR;
        colorF.fG *= alpha * m_state.factor.fG;
        colorF.fB *= alpha * m_state.factor.fB;
        colorF.fA *= alpha;
    }
    else
    {
        colorF.fA *= m_state.opacity * m_state.factor.fA;
        colorF.fR *= colorF.fA * m_state.factor.fR;
        colorF.fG *= colorF.fA * m_state.factor.fG;
        colorF.fB *= colorF.fA * m_state.factor.fB;
    }

    if (m_state.blendMode == RBlendMode::DstIn && colorF.fA >= 1.f)
    {
        device()->log(CZTrace, "Skipping drawColor(DstIn, alpha = 1.f): multiplying dst by 1.f is a noop");
        return true;
    }

    prog->bind();
    glBindFramebuffer(GL_FRAMEBUFFER, fb.value());
    glUniform4fv(prog->loc().color, 1, (GLfloat*)&colorF);

    // posProj
    SkScalar matVals[9];
    calcPosProj(surface.get(), fb.value() == 0, matVals);
    glUniformMatrix3fv(prog->loc().posProj, 1, GL_FALSE, matVals);

    std::vector<GLfloat> vbo { genVBO(region) };
    glEnableVertexAttribArray(prog->loc().pos);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, vbo.data());
    const auto size { surface->image()->size() };
    glViewport(0, 0, size.width(), size.height());

    if (m_state.blendMode == RBlendMode::Src)
    {
        glDisable(GL_BLEND);
    }
    else if (m_state.blendMode == RBlendMode::SrcOver)
    {
        if (colorF.fA >= 1.f)
        {
            glDisable(GL_BLEND);
        }
        else
        {
            glEnable(GL_BLEND);
            glBlendEquation(GL_FUNC_ADD);
            glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        }
    }
    else // DstIn
    {
        glEnable(GL_BLEND);
        glBlendEquation(GL_FUNC_ADD);
        glBlendFunc(GL_ZERO, GL_SRC_ALPHA);
    }

    setScissors(surface.get(), fb == 0, region);
    glDrawArrays(GL_TRIANGLES, 0, region.computeRegionComplexity() * 6);
    glDisableVertexAttribArray(prog->loc().pos);
    return true;
}

CZBitset<RGLShader::Features> RGLPainter::calcFeatures(std::shared_ptr<RImage> image, std::shared_ptr<RImage> mask) const noexcept
{
    CZBitset<RGLShader::Features> features {};

    if (image)
    {
        features.add(RGLShader::HasImage);
        features.setFlag(RGLShader::HasMask, mask != nullptr);

        if (m_state.blendMode != RBlendMode::DstIn)
        {
            features.setFlag(RGLShader::HasFactorR, m_state.factor.fR != 1.f);
            features.setFlag(RGLShader::HasFactorG, m_state.factor.fG != 1.f);
            features.setFlag(RGLShader::HasFactorB, m_state.factor.fB != 1.f);
        }

        features.setFlag(RGLShader::HasFactorA, m_state.factor.fA != 1.f || m_state.opacity < 1.f);

        /* If the image is premult and only has A, do color * A in shader */
        features.setFlag(RGLShader::PremultSrc,
            image->alphaType() == kPremul_SkAlphaType &&
            features.has(RGLShader::HasFactorA) &&
            !features.has(RGLShader::HasFactorR | RGLShader::HasFactorG | RGLShader::HasFactorB));
    }

    /* drawColor features are always 0 since the final color is pre-calculated in the CPU */

    return features;
}

std::shared_ptr<RGLPainter> RGLPainter::Make(RGLDevice *device) noexcept
{
    auto painter { std::shared_ptr<RGLPainter>(new RGLPainter(device)) };

    if (painter->init())
        return painter;

    return {};
}

bool RGLPainter::init() noexcept
{
    auto current { RGLMakeCurrent::FromDevice(device(), false) };
    auto testA { RGLProgram::GetOrMake(this, RGLShader::HasImage) };
    auto testB { RGLProgram::GetOrMake(this, 0) };
    return testA != nullptr && testB != nullptr;
}

bool RGLPainter::setSurface(std::shared_ptr<RSurface> surface) noexcept
{
    m_surface = surface;
    return true;
}
