#ifndef RGLPROGRAM_H
#define RGLPROGRAM_H

#include <CZ/Ream/GL/RGLShader.h>
#include <CZ/Ream/RObject.h>
#include <GLES2/gl2.h>

/**
 * @brief A linked OpenGL ES program (vertex + fragment shader) for a given feature set.
 *
 * Each program is built from an RGLShader pair selected by a set of RGLShader::Features flags.
 * Programs are cached per feature set in the RGLDevice's per-thread context data, so a given
 * feature combination is only linked once per device/context.
 */
class CZ::RGLProgram final : public RObject
{
public:
    /**
     * @brief Cached locations of the program's vertex attributes and uniforms.
     *
     * Locations that do not apply to the program's feature set are left unset.
     */
    struct Locations
    {
        GLint pos;          ///< `pos` vertex attribute (vertex position).
        GLint posProj;      ///< `posProj` uniform (position projection matrix).

        GLint image;        ///< `image` sampler uniform (source texture).
        GLint imageProj;    ///< `imageProj` uniform (source texture-coordinate matrix).

        GLint mask;         ///< `mask` sampler uniform (mask texture).
        GLint maskProj;     ///< `maskProj` uniform (mask texture-coordinate matrix).

        GLint factorR;      ///< `factorR` uniform (red channel multiplier).
        GLint factorG;      ///< `factorG` uniform (green channel multiplier).
        GLint factorB;      ///< `factorB` uniform (blue channel multiplier).
        GLint factorA;      ///< `factorA` uniform (alpha channel multiplier).

        GLint pixelSize;    ///< `pixelSize` uniform (texel size, used by effect shaders).
    };

    /**
     * @brief Returns the cached program for the given device and feature set, building it if needed.
     *
     * @param device   The target device (uses its per-thread context data).
     * @param features The RGLShader feature flags describing the desired program.
     * @return The program, or `nullptr` if compilation/linking failed.
     */
    static std::shared_ptr<RGLProgram> GetOrMake(RGLDevice *device, CZBitset<RGLShader::Features> features) noexcept;

    /**
     * @brief Deletes the underlying GL program object.
     */
    ~RGLProgram() noexcept;

    /**
     * @brief Returns the attribute/uniform locations resolved at link time.
     */
    const Locations &loc() const noexcept { return m_loc; }

    /**
     * @brief Returns the GL program object name.
     */
    GLuint id() const noexcept { return m_id; }

    /**
     * @brief Returns the feature flags this program was built for.
     */
    CZBitset<RGLShader::Features> features() const noexcept { return m_features; }

    /**
     * @brief Returns the device this program belongs to.
     */
    RGLDevice *device() const noexcept { return m_device; }

    /**
     * @brief Makes this program the current one (`glUseProgram`).
     */
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
