#ifndef RGLSHADER_H
#define RGLSHADER_H

#include <CZ/Ream/RObject.h>
#include <CZ/Core/CZBitset.h>
#include <GLES2/gl2.h>
#include <memory>

/**
 * @brief A compiled OpenGL ES shader specialized for a set of features.
 *
 * The GL backend uses a single über-shader whose behavior is controlled by preprocessor defines
 * derived from a Features bitmask. RGLShader compiles one variant per (feature set, shader type)
 * and caches it in the RGLDevice's per-thread context data. Programs (RGLProgram) combine a vertex
 * and a fragment variant sharing the same feature set.
 */
class CZ::RGLShader final : public RObject
{
public:
    /**
     * @brief Feature flags controlling shader specialization.
     *
     * The lowest 2 bits encode the blend mode, the middle bits are boolean capabilities, and the
     * upper 4 bits encode an optional image effect.
     */
    enum Features : UInt32
    {
        /* The lowest 2 bits represent the blend mode */
        BlendSrc            = 0,        ///< Source blend mode (replace destination).
        BlendSrcOver        = 1,        ///< Source-over blend mode.
        BlendDstIn          = 0x3,      ///< Destination-in blend mode (mask by source alpha).

        HasImage            = 1u << 2,  ///< Samples a source image texture.
        ImageExternal       = 1u << 3,  ///< The source texture is an external (GL_TEXTURE_EXTERNAL_OES) sampler.
        HasMask             = 1u << 4,  ///< Samples a mask texture.
        MaskExternal        = 1u << 5,  ///< The mask texture is an external sampler.
        ReplaceImageColor   = 1u << 6,  ///< Replaces the image RGB with a solid color, keeping its alpha.
        HasFactorR          = 1u << 7,  ///< Applies the red-channel multiplier.
        HasFactorG          = 1u << 8,  ///< Applies the green-channel multiplier.
        HasFactorB          = 1u << 9,  ///< Applies the blue-channel multiplier.
        HasFactorA          = 1u << 10, ///< Applies the alpha-channel multiplier.
        PremultSrc          = 1u << 11, ///< The source color is premultiplied alpha.
        HasPixelSize        = 1u << 12, ///< Provides the `pixelSize` uniform (texel size for effects).

        /* The upper 4 bits represent effects */
        VibrancyH           = 1u << 28, ///< Horizontal vibrancy blur pass.
        VibrancyLightV      = 2u << 28, ///< Vertical vibrancy blur pass with light-tone saturation.
        VibrancyDarkV       = 3u << 28, ///< Vertical vibrancy blur pass with dark-tone saturation.
    };

    /// Subset of Features that affect the vertex shader (the rest only affect the fragment shader).
    static constexpr CZBitset<Features> VertFeatures { HasImage | HasMask };

    /**
     * @brief Returns the cached shader for the given device, feature set, and type, compiling it if needed.
     *
     * @param device   The target device (uses its per-thread context data).
     * @param features The feature flags describing the desired shader variant.
     * @param type     The GL shader stage (`GL_VERTEX_SHADER` or `GL_FRAGMENT_SHADER`).
     * @return The compiled shader, or `nullptr` on compilation failure.
     */
    static std::shared_ptr<RGLShader> GetOrMake(RGLDevice *device, CZBitset<Features> features, GLenum type) noexcept;

    /**
     * @brief Deletes the underlying GL shader object.
     */
    ~RGLShader() noexcept;

    /**
     * @brief Returns the GL shader object name.
     */
    GLuint id() const noexcept { return m_id; }

    /**
     * @brief Returns the GL shader stage (`GL_VERTEX_SHADER` or `GL_FRAGMENT_SHADER`).
     */
    GLenum type() const noexcept { return m_type; }

    /**
     * @brief Returns the feature flags this shader was compiled for.
     */
    CZBitset<Features> features() const noexcept { return m_features; };

    /**
     * @brief Returns the device this shader belongs to.
     */
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
