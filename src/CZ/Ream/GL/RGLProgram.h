#ifndef RGLPROGRAM_H
#define RGLPROGRAM_H

#include <CZ/Ream/GL/RGLShader.h>
#include <CZ/Ream/RObject.h>
#include <GLES2/gl2.h>

class CZ::RGLProgram final : public RObject
{
public:
    struct Locations
    {
        GLint pos;
        GLint posProj;

        GLint image;
        GLint imageProj;

        GLint mask;
        GLint maskProj;

        GLint color;
        GLint factorR;
        GLint factorG;
        GLint factorB;
        GLint factorA;
    };

    static std::shared_ptr<RGLProgram> GetOrMake(RGLPainter *painter, CZBitset<RGLShader::Features> features) noexcept;
    ~RGLProgram() noexcept;
    const Locations &loc() const noexcept { return m_loc; }
    GLuint id() const noexcept { return m_id; }
    CZBitset<RGLShader::Features> features() const noexcept { return m_features; }
    RGLPainter *painter() const noexcept { return m_painter; }
    void bind() noexcept;
private:
    RGLProgram(RGLPainter *painter, CZBitset<RGLShader::Features> features) noexcept;
    bool link() noexcept;
    Locations m_loc {};
    CZBitset<RGLShader::Features> m_features;
    std::shared_ptr<RGLShader> m_frag;
    std::shared_ptr<RGLShader> m_vert;
    GLuint m_id {};
    RGLPainter *m_painter;
};

#endif // RGLPROGRAM_H
