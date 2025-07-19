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
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

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
    if (blendMode() == RBlendMode::SrcOver && (factor().fA <= 0.f || opacity() <= 0.f))
        return true;

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

        if (mask->alphaType() == SkAlphaType::kOpaque_SkAlphaType)
        {
            RLog(CZWarning, CZLN, "The mask's alpha type is Opaque. Ignoring it...");
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

    const auto current { RGLMakeCurrent::FromDevice(device(), true) };
    const auto features { calcDrawImageFeatures(image, &tex, mask ? &maskTex : nullptr) };
    const auto prog { RGLProgram::GetOrMake(this, features) };

    if (!prog)
    {
        device()->log(CZError, CZLN, "Failed to create a GL program with the required features");
        return false;
    }

    // Color
    SkColor4f colorF { m_state.factor };

    if (blendMode() != RBlendMode::DstIn && features.has(RGLShader::ReplaceImageColor))
    {
        const SkColor4f replaceColorF { SkColor4f::FromColor(color()) };        
        colorF.fA *= m_state.opacity;
        colorF.fR *= replaceColorF.fR * colorF.fA;
        colorF.fG *= replaceColorF.fG * colorF.fA;
        colorF.fB *= replaceColorF.fB * colorF.fA;
    }
    else
    {
        colorF.fA *= m_state.opacity;
    }

    /* Skip dummy user operations */
    if (image->alphaType() == kOpaque_SkAlphaType && !mask)
    {
        if (blendMode() == RBlendMode::DstIn)
        {
            if (colorF.fA == 1.f)
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
        } /* Replacing the color of an opaque image is the same as drawing a solid color */
        else if (features.has(RGLShader::ReplaceImageColor))
        {
            save(); // Keep everything except color.a
            setColor(SkColorSetA(color(), 255));
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

    if (blendMode() == RBlendMode::Src)
    {
        if (features.has(RGLShader::ReplaceImageColor))
        {
            // The color is premult and HasFactorA is not set
            // but RGB must be multiplied by image.a and/or mask.a
            glEnable(GL_BLEND);
            glBlendEquation(GL_FUNC_ADD);
            glBlendFuncSeparate(GL_SRC_ALPHA, GL_ZERO, GL_ONE, GL_ZERO);
        }
        else
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
    }
    else if (blendMode() == RBlendMode::SrcOver)
    {
        if (features.has(RGLShader::ReplaceImageColor))
        {
            // The color is premult and HasFactorA is not set
            // but RGB must be multiplied by image.a and/or mask.a
            glEnable(GL_BLEND);
            glBlendEquation(GL_FUNC_ADD);
            glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        }
        else
        {
            if (image->alphaType() == kOpaque_SkAlphaType)
            {
                if (colorF.fA >= 1.f && !mask)
                    glDisable(GL_BLEND);
                else
                {
                    glEnable(GL_BLEND);
                    glBlendEquation(GL_FUNC_ADD);
                    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
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
    }
    else // DstIn
    {
        /* We only care about A so the alphaType() doesn't matter.
         * This function exits earlier if the alpha type is Opaque
         * finalAlpha = 1.f and there is no mask */
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
    if (blendMode() == RBlendMode::SrcOver && (SkColorGetA(color()) == 0 || factor().fA <= 0.f || opacity() <= 0.f))
        return true;

    const auto surface { m_surface.lock() };

    if (!surface || !surface->image())
        return false;

    const SkIRect clipRect { SkIRect::MakePtSize(surface->pos(), surface->size()) };

    SkRegion region;

    if (!region.op(clipRect, userRegion, SkRegion::kIntersect_Op))
        return true; // Empty

    const auto fb { surface->image()->asGL()->glFb(device()) };

    if (!fb.has_value())
    {
        device()->log(CZError, CZLN, "Failed to get GL framebuffer from RGLImage");
        return false;
    }

    const auto current { RGLMakeCurrent::FromDevice(device(), true) };

    // Always converted to premultiplied alpha
    const SkColor4f colorF { calcDrawColorColor() };

    if (m_state.blendMode == RBlendMode::DstIn && colorF.fA >= 1.f)
    {
        device()->log(CZTrace, "Skipping drawColor(DstIn, alpha = 1.f): multiplying dst by 1.f is a noop");
        return true;
    }

    const auto features { calcDrawColorFeatures(colorF.fA) };
    const auto prog { RGLProgram::GetOrMake(this, features) };

    if (!prog)
    {
        device()->log(CZError, CZLN, "Failed to create a GL program with the required features");
        return false;
    }

    prog->bind();
    glBindFramebuffer(GL_FRAMEBUFFER, fb.value());
    setDrawColorUniforms(features, prog, colorF);

    // posProj
    SkScalar matVals[9];
    calcPosProj(surface.get(), fb.value() == 0, matVals);
    glUniformMatrix3fv(prog->loc().posProj, 1, GL_FALSE, matVals);

    std::vector<GLfloat> vbo { genVBO(region) };
    glEnableVertexAttribArray(prog->loc().pos);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, vbo.data());

    setDrawColorBlendFunc(features);

    glViewport(0, 0, surface->image()->size().width(), surface->image()->size().height());
    setScissors(surface.get(), fb == 0, region);

    glDrawArrays(GL_TRIANGLES, 0, region.computeRegionComplexity() * 6);
    glDisableVertexAttribArray(prog->loc().pos);
    return true;
}

void RGLPainter::beginPass() noexcept
{
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glEnableVertexAttribArray(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1.0f);
    glEnable(GL_BLEND);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_SCISSOR_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DITHER);
    glDisable(GL_POLYGON_OFFSET_FILL);
    glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    glDisable(GL_SAMPLE_COVERAGE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glCullFace(GL_BACK);
    glLineWidth(1);
    glHint(GL_GENERATE_MIPMAP_HINT, GL_FASTEST);
    glPolygonOffset(0, 0);
    glDepthFunc(GL_LESS);
    glDepthRangef(0, 1);
    glStencilMask(1);
    glDepthMask(GL_FALSE);
    glFrontFace(GL_CCW);
    glBlendColor(0, 0, 0, 0);
    glBlendEquation(GL_FUNC_ADD);
}

CZBitset<RGLShader::Features> RGLPainter::calcDrawImageFeatures(std::shared_ptr<RImage> image, RGLTexture *imageTex, RGLTexture *maskTex) const noexcept
{
    assert(image);
    assert(imageTex);

    CZBitset<RGLShader::Features> features {};
    features.add(RGLShader::HasImage);
    features.setFlag(RGLShader::ImageExternal, imageTex->target == GL_TEXTURE_EXTERNAL_OES);

    if (maskTex) // If the mask is opaque, this will always be nullptr
    {
        features.add(RGLShader::HasMask);
        features.setFlag(RGLShader::MaskExternal, maskTex->target == GL_TEXTURE_EXTERNAL_OES);
    }

    switch (blendMode())
    {
    case RBlendMode::Src:
        features.add(RGLShader::BlendSrc);

        if (m_state.options.has(ReplaceImageColor))
        {
            // UNUSED: PremultSrc, HasFactorA
            features.add(RGLShader::ReplaceImageColor | RGLShader::HasFactorR | RGLShader::HasFactorG | RGLShader::HasFactorB);
        }
        else
        {
            features.setFlag(RGLShader::PremultSrc, image->alphaType() == kPremul_SkAlphaType);
            features.setFlag(RGLShader::HasFactorR, m_state.factor.fR != 1.f);
            features.setFlag(RGLShader::HasFactorG, m_state.factor.fG != 1.f);
            features.setFlag(RGLShader::HasFactorB, m_state.factor.fB != 1.f);
            features.setFlag(RGLShader::HasFactorA, m_state.factor.fA * m_state.opacity != 1.f);
        }
        break;
    case RBlendMode::SrcOver:
        features.add(RGLShader::BlendSrcOver);

        if (m_state.options.has(ReplaceImageColor))
        {
            // UNUSED: PremultSrc, HasFactorA
            features.add(RGLShader::ReplaceImageColor | RGLShader::HasFactorR | RGLShader::HasFactorG | RGLShader::HasFactorB);
        }
        else
        {
            features.setFlag(RGLShader::PremultSrc, image->alphaType() == kPremul_SkAlphaType);
            features.setFlag(RGLShader::HasFactorR, m_state.factor.fR != 1.f);
            features.setFlag(RGLShader::HasFactorG, m_state.factor.fG != 1.f);
            features.setFlag(RGLShader::HasFactorB, m_state.factor.fB != 1.f);
            features.setFlag(RGLShader::HasFactorA, m_state.factor.fA * m_state.opacity != 1.f);
        }
        break;
    case RBlendMode::DstIn:
        // UNUSED: PremultSrc, ReplaceImageColor, HasFactor{R,G,B}
        features.add(RGLShader::BlendDstIn);
        features.setFlag(RGLShader::HasFactorA, m_state.factor.fA * m_state.opacity != 1.f);
        break;
    }

    return features;
}

CZBitset<RGLShader::Features> RGLPainter::calcDrawColorFeatures(SkScalar finalAlpha) const noexcept
{
    CZBitset<RGLShader::Features> features {};

    switch (blendMode())
    {
    case RBlendMode::Src: // glBlend disabled
        features.set(RGLShader::HasFactorR | RGLShader::HasFactorG | RGLShader::HasFactorB);
        break;
    case RBlendMode::SrcOver:
        if (finalAlpha == 1.f) // glBlend disabled
            features.set(RGLShader::HasFactorR | RGLShader::HasFactorG | RGLShader::HasFactorB);
        else
            features.set(RGLShader::HasFactorR | RGLShader::HasFactorG | RGLShader::HasFactorB | RGLShader::HasFactorA);
        break;
    case RBlendMode::DstIn: // Only alpha is used
        features.set(RGLShader::HasFactorA);
        break;
    };

    return features;
}

void RGLPainter::setDrawColorUniforms(CZBitset<RGLShader::Features> features, std::shared_ptr<RGLProgram> prog, const SkColor4f &colorF) const noexcept
{
    if (features.has(RGLShader::HasFactorR))
        glUniform1f(prog->loc().factorR, colorF.fR);

    if (features.has(RGLShader::HasFactorG))
        glUniform1f(prog->loc().factorG, colorF.fG);

    if (features.has(RGLShader::HasFactorB))
        glUniform1f(prog->loc().factorB, colorF.fB);

    if (features.has(RGLShader::HasFactorA))
        glUniform1f(prog->loc().factorA, colorF.fA);
}

void RGLPainter::setDrawColorBlendFunc(CZBitset<RGLShader::Features> features) const noexcept
{
    switch (blendMode())
    {
    case RBlendMode::Src:
        glDisable(GL_BLEND);
        break;
    case RBlendMode::SrcOver:
        if (features.has(RGLShader::HasFactorA))
        {
            glEnable(GL_BLEND);
            glBlendEquation(GL_FUNC_ADD);
            glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        }
        else
            glDisable(GL_BLEND);
        break;
    case RBlendMode::DstIn:
        glEnable(GL_BLEND);
        glBlendEquation(GL_FUNC_ADD);
        glBlendFunc(GL_ZERO, GL_SRC_ALPHA);
        break;
    };
}

SkColor4f RGLPainter::calcDrawColorColor() const noexcept
{
    // Always converted to premultiplied alpha
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

    return colorF;
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
    return true;
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
