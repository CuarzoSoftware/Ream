#include <CZ/Ream/GL/RGLMakeCurrent.h>
#include <CZ/Ream/GL/RGLShader.h>
#include <CZ/Ream/GL/RGLPainter.h>
#include <CZ/Ream/GL/RGLDevice.h>
#include <format>

#include <string>
#include <sstream>
#include <unordered_map>
#include <stack>
#include <regex>
#include <iostream>

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
    uniform IMAGE_SAMPLER image;

    #ifdef HAS_MASK
        varying vec2 maskCord;
        uniform MASK_SAMPLER mask;
    #endif
#endif

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

void main()
{

#ifdef HAS_IMAGE

    // Src and SrcOver Blend Mode
    #if BLEND_MODE == 0 || BLEND_MODE == 1

        #ifdef REPLACE_IMAGE_COLOR

            // factorRGB must not be premult by factorA

            gl_FragColor = vec4(factorR, factorG, factorB, texture2D(image, imageCord).a);

            #ifdef HAS_MASK
                gl_FragColor.a *= texture2D(mask, maskCord).a;
            #endif

            #ifdef HAS_A
                gl_FragColor.a *= factorA;
            #endif

        #else // REPLACE COLOR DISABLED

            gl_FragColor = texture2D(image, imageCord);

            // factorRGB must always be unpremult

            #ifdef HAS_R
                gl_FragColor.r *= factorR;
            #endif

            #ifdef HAS_G
                gl_FragColor.g *= factorG;
            #endif

            #ifdef HAS_B
                gl_FragColor.b *= factorB;
            #endif

            #ifdef PREMULT_SRC

                #ifdef HAS_A

                    #ifdef HAS_MASK
                        gl_FragColor *= (factorA * texture2D(mask, maskCord).a);
                    #else
                        gl_FragColor *= factorA;
                    #endif

                #else // NO ALPHA

                    #ifdef HAS_MASK
                        gl_FragColor *= texture2D(mask, maskCord).a;
                    #endif

                #endif

            #else // UNPREMULT SRC

                #ifdef HAS_A
                    gl_FragColor.a *= factorA;
                #endif

                #ifdef HAS_MASK
                    gl_FragColor.a *= texture2D(mask, maskCord).a;
                #endif

            #endif // PREMULT_SRC

        #endif // REPLACE_IMAGE_COLOR

    // DstIn Blend Mode
    #else

        // Unaffected by REPLACE_IMAGE_COLOR and PREMULT_SRC

        gl_FragColor.a = texture2D(image, imageCord).a;

        #ifdef HAS_MASK
            gl_FragColor.a *= texture2D(mask, maskCord).a;
        #endif

        #ifdef HAS_A
            gl_FragColor.a *= factorA;
        #endif

    #endif

#else // COLOR MODE (Unaffected by BLEND_MODE)

    #ifdef HAS_R
        gl_FragColor.r = factorR;
    #endif

    #ifdef HAS_G
        gl_FragColor.g = factorG;
    #endif

    #ifdef HAS_B
        gl_FragColor.b = factorB;
    #endif

    #ifdef HAS_A
        gl_FragColor.a = factorA;
    #else
        gl_FragColor.a = 1.0;
    #endif

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

    // device->log(CZTrace, "New {} Shader: \n{}", type == GL_VERTEX_SHADER ? "vertex" : "fragment" , source);

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
        std::format("{}{}{}{}{}{}{}{}{}{}{}#define BLEND_MODE {}\n",
            !m_features.has(ImageExternal | MaskExternal) ? "" : "#extension GL_OES_EGL_image_external : require\n",
            !m_features.has(ImageExternal)                ? "#define IMAGE_SAMPLER sampler2D\n" : "#define IMAGE_SAMPLER samplerExternalOES\n",
            !m_features.has(MaskExternal)                 ? "#define MASK_SAMPLER sampler2D\n" : "#define MASK_SAMPLER samplerExternalOES\n", 
            !m_features.has(ReplaceImageColor)            ? "" : "#define REPLACE_IMAGE_COLOR\n",
            !m_features.has(PremultSrc)                   ? "" : "#define PREMULT_SRC\n",
            !m_features.has(HasImage)                     ? "" : "#define HAS_IMAGE\n",
            !m_features.has(HasMask)                      ? "" : "#define HAS_MASK\n",
            !m_features.has(HasFactorR)                   ? "" : "#define HAS_R\n",
            !m_features.has(HasFactorG)                   ? "" : "#define HAS_G\n",
            !m_features.has(HasFactorB)                   ? "" : "#define HAS_B\n",
            !m_features.has(HasFactorA)                   ? "" : "#define HAS_A\n",
            UInt32(m_features.get() & 0x3)) // Blend Mode
    };

    m_id = CompileShader(painter()->device(), type(), featuresStr);
    return m_id != 0;
}
