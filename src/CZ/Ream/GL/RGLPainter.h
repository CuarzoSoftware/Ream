#ifndef RGLPAINTER_H
#define RGLPAINTER_H

#include <CZ/Ream/GL/RGLTexture.h>
#include <CZ/Ream/RPainter.h>
#include <CZ/Ream/GL/RGLShader.h>
#include <GLES2/gl2.h>

class CZ::RGLPainter final : public RPainter
{
public:
    bool drawImage(const RDrawImageInfo& image, const SkRegion *region = nullptr, const RDrawImageInfo* mask = nullptr) noexcept override;
    bool drawColor(const SkRegion &region) noexcept override;
    RGLDevice *device() const noexcept { return (RGLDevice*)m_device; }

private:
    CZBitset<RGLShader::Features> calcDrawImageFeatures(std::shared_ptr<RImage> image, RGLTexture *imageTex, RGLTexture *maskTex) const noexcept;
    CZBitset<RGLShader::Features> calcDrawColorFeatures(SkScalar finalAlpha) const noexcept;
    SkColor4f calcDrawColorColor() const noexcept;
    void setDrawColorUniforms(CZBitset<RGLShader::Features> features, std::shared_ptr<RGLProgram> prog, const SkColor4f &colorF) const noexcept;
    void setDrawColorBlendFunc(CZBitset<RGLShader::Features> features) const noexcept;

    std::vector<GLfloat> genVBO(const SkRegion &region) const noexcept;
    SkRegion calcDrawImageRegion(RSurface *surface, const RDrawImageInfo &imageInfo, const SkRegion *clip, const RDrawImageInfo *maskInfo) const noexcept;
    void calcPosProj(RSurface *surface, bool flipY, SkScalar *outMat) const noexcept;
    void calcImageProj(const RDrawImageInfo &info, SkScalar *outMat) const noexcept;
    void setScissors(RSurface *surface, bool flipY, const SkRegion &region) const noexcept;
    void bindTexture(RGLTexture tex, GLuint uniform, const RDrawImageInfo &info, GLuint slot) const noexcept;
    friend class RGLDevice;
    friend class RGLShader;
    friend class RGLProgram;
    static std::shared_ptr<RGLPainter> Make(RGLDevice *device) noexcept;
    bool init() noexcept;
    RGLPainter(RGLDevice *device) noexcept : RPainter((RDevice*)device) {};
    bool setSurface(std::shared_ptr<RSurface> surface) noexcept;
    std::unordered_map<UInt32, std::shared_ptr<RGLShader>> m_vertShaders;
    std::unordered_map<UInt32, std::shared_ptr<RGLShader>> m_fragShaders;
    std::unordered_map<UInt32, std::shared_ptr<RGLProgram>> m_programs;
};

#endif // RGLPAINTER_H
