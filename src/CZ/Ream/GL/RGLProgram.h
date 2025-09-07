#ifndef RGLPROGRAM_H
#define RGLPROGRAM_H

#include <CZ/Ream/GL/RGLShader.h>
#include <CZ/Ream/RObject.h>
#include <GLES2/gl2.h>

// GL program (vert + frag shader) given a set of features
// Stored in the RGDevice thread data
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

        GLint factorR;
        GLint factorG;
        GLint factorB;
        GLint factorA;

        GLint pixelSize;
    };

    static std::shared_ptr<RGLProgram> GetOrMake(RGLDevice *device, CZBitset<RGLShader::Features> features) noexcept;
    ~RGLProgram() noexcept;
    const Locations &loc() const noexcept { return m_loc; }
    GLuint id() const noexcept { return m_id; }
    CZBitset<RGLShader::Features> features() const noexcept { return m_features; }
    RGLDevice *device() const noexcept { return m_device; }
    void bind() noexcept;
private:
    RGLProgram(RGLDevice *device, CZBitset<RGLShader::Features> features) noexcept;
    bool link() noexcept;
    Locations m_loc {};
    CZBitset<RGLShader::Features> m_features;
    std::shared_ptr<RGLShader> m_frag;
    std::shared_ptr<RGLShader> m_vert;
    GLuint m_id {};
    RGLDevice *m_device;
};

#endif // RGLPROGRAM_H
