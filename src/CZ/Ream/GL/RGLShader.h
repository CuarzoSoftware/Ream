#ifndef RGLSHADER_H
#define RGLSHADER_H

#include <CZ/Ream/RObject.h>
#include <CZ/Core/CZBitset.h>
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
        PremultSrc          = 1u << 11,
        HasPixelSize        = 1u << 12,

        /* The upper 4 bits represent effects */
        VibrancyH           = 1u << 28,
        VibrancyLightV      = 2u << 28,
        VibrancyDarkV       = 3u << 28,
    };

    static constexpr CZBitset<Features> VertFeatures { HasImage | HasMask };

    static std::shared_ptr<RGLShader> GetOrMake(RGLDevice *device, CZBitset<Features> features, GLenum type) noexcept;

    ~RGLShader() noexcept;
    GLuint id() const noexcept { return m_id; }
    GLenum type() const noexcept { return m_type; }
    CZBitset<Features> features() const noexcept { return m_features; };
    RGLDevice *device() const noexcept { return m_device; };

private:
    RGLShader(RGLDevice *device, CZBitset<Features> features, GLenum type) noexcept :
        m_type(type), m_features(features), m_device(device)
    {}

    bool build() noexcept;
    GLuint m_id;
    GLenum m_type;
    CZBitset<Features> m_features;
    RGLDevice *m_device;
};

#endif // RGLSHADER_H
