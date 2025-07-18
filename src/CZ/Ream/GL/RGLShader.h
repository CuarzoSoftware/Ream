#ifndef RGLSHADER_H
#define RGLSHADER_H

#include <CZ/Ream/RObject.h>
#include <CZ/CZBitset.h>
#include <GLES2/gl2.h>
#include <memory>

class CZ::RGLShader final : public RObject
{
public:
    enum Features : UInt32
    {
        /* The lowest 2 bits represent the blend mode */
        BlendSrc            = 0,
        BlendSrcOver        = 1,
        BlendDstIn          = 0x3,

        HasImage            = 1u << 2,
        ImageExternal       = 1u << 3,
        HasMask             = 1u << 4,
        MaskExternal        = 1u << 5,
        ReplaceImageColor   = 1u << 6,
        HasFactorR          = 1u << 7,
        HasFactorG          = 1u << 8,
        HasFactorB          = 1u << 9,
        HasFactorA          = 1u << 10,
        PremultSrc          = 1u << 11
    };

    static constexpr CZBitset<Features> VertFeatures { HasImage | HasMask };

    static std::shared_ptr<RGLShader> GetOrMake(RGLPainter *painter, CZBitset<Features> features, GLenum type) noexcept;

    ~RGLShader() noexcept;
    GLuint id() const noexcept { return m_id; }
    GLenum type() const noexcept { return m_type; }
    CZBitset<Features> features() const noexcept { return m_features; };
    RGLPainter *painter() const noexcept { return m_painter; };

private:
    RGLShader(RGLPainter *painter, CZBitset<Features> features, GLenum type) noexcept :
        m_type(type), m_features(features), m_painter(painter)
    {}

    bool build() noexcept;
    GLuint m_id;
    GLenum m_type;
    CZBitset<Features> m_features;
    RGLPainter *m_painter;
};

#endif // RGLSHADER_H
