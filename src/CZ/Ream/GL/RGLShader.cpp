#include <CZ/Ream/GL/RGLMakeCurrent.h>
#include <CZ/Ream/GL/RGLShader.h>
#include <CZ/Ream/GL/RGLPainter.h>
#include <CZ/Ream/GL/RGLDevice.h>
#include <format>

using namespace CZ;

const char *v = R"(
uniform mat3 posProj;
attribute vec2 pos;

#ifdef HAS_IMAGE
    uniform mat3 imageProj;
    varying vec2 imageCord;

    #ifdef HAS_MASK
        uniform mat3 maskProj;
        varying vec2 maskCord;
    #endif
#endif

void main() {
    vec3 pos3 = vec3(pos, 1.0);
    gl_Position = vec4(pos3 * posProj, 1.0);

#ifdef HAS_IMAGE
    imageCord = (pos3 * imageProj).xy;

    #ifdef HAS_MASK
        maskCord = (pos3 * maskProj).xy;
    #endif
#endif
}
)";

const char *f = R"(
#ifdef GL_FRAGMENT_PRECISION_HIGH
    precision highp float;
#else
    precision mediump float;
#endif

#ifdef HAS_IMAGE
    varying vec2 imageCord;
    uniform sampler2D image;

    #ifdef HAS_R
        uniform float factorR;
    #endif

    #ifdef HAS_G
        uniform float factorG;
    #endif

    #ifdef HAS_B
        uniform float factorB;
    #endif

    #ifdef HAS_A
        uniform float factorA;
    #endif

    #ifdef HAS_MASK
        varying vec2 maskCord;
        uniform sampler2D mask;
    #endif
#else
    uniform vec4 color;
#endif

void main()
{
#ifdef HAS_IMAGE

    #ifdef PREMULT_SRC
        #ifdef HAS_MASK
            gl_FragColor = texture2D(image, imageCord) * (factorA * texture2D(mask, maskCord).a);
        #else
            gl_FragColor = texture2D(image, imageCord) * factorA;
        #endif
    #else
        gl_FragColor = texture2D(image, imageCord);

        #ifdef HAS_R
            gl_FragColor.r *= factorR;
        #endif

        #ifdef HAS_G
            gl_FragColor.g *= factorG;
        #endif

        #ifdef HAS_B
            gl_FragColor.b *= factorB;
        #endif

        #ifdef HAS_A
            gl_FragColor.a *= factorA;
        #endif

        #ifdef HAS_MASK
            gl_FragColor.a *= texture2D(mask, maskCord).a;
        #endif
    #endif
#else
    gl_FragColor = color;
#endif
}
)";

std::shared_ptr<RGLShader> RGLShader::GetOrMake(RGLPainter *painter, CZBitset<Features> features, GLenum type) noexcept
{
    auto current { RGLMakeCurrent::FromDevice(painter->device(), false) };
    auto *map { &painter->m_fragShaders };

    if (type == GL_VERTEX_SHADER)
    {
        features = features.get() & VertFeatures;
        map = &painter->m_vertShaders;
    }

    auto it { map->find(features) };

    if (it != map->end())
        return it->second;

    auto shader { std::shared_ptr<RGLShader>(new RGLShader(painter, features, type)) };

    if (shader->build())
    {
        map->emplace(features, shader);
        return shader;
    }

    return {};
}

RGLShader::~RGLShader() noexcept
{
    if (m_id == 0)
        return;

    auto current { RGLMakeCurrent::FromDevice(painter()->device(), false) };
    glDeleteShader(m_id);
}

static GLuint CompileShader(RGLDevice *device, GLenum type, const std::string &features)
{
    std::string source;

    if (type == GL_VERTEX_SHADER)
        source = features + v;
    else
        source = features + f;

    GLuint shader;
    GLint compiled;
    shader = glCreateShader(type);
    const char *src { source.c_str() };
    glShaderSource(shader, 1, &src, NULL);
    glCompileShader(shader);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);

    if (!compiled)
    {
        GLint infoLen { 0 };
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
        std::vector<GLchar> errorMsg;
        errorMsg.resize(infoLen);
        glGetShaderInfoLog(shader, infoLen, &infoLen, errorMsg.data());
        device->log(CZError, "Failed to compile shader: {}", (const char*)errorMsg.data());
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

bool RGLShader::build() noexcept
{
    const std::string featuresStr {
        std::format("{}{}{}{}{}{}{}",
            !m_features.has(PremultSrc) ? "" : "#define PREMULT_SRC\n",
            !m_features.has(HasImage)   ? "" : "#define HAS_IMAGE\n",
            !m_features.has(HasMask)    ? "" : "#define HAS_MASK\n",
            !m_features.has(HasFactorR) ? "" : "#define HAS_R\n",
            !m_features.has(HasFactorG) ? "" : "#define HAS_G\n",
            !m_features.has(HasFactorB) ? "" : "#define HAS_B\n",
            !m_features.has(HasFactorA) ? "" : "#define HAS_A\n")
    };

    m_id = CompileShader(painter()->device(), type(), featuresStr);
    return m_id != 0;
}
