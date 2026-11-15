#include <CZ/Ream/GL/RGLMakeCurrent.h>
#include <CZ/Ream/GL/RGLShader.h>
#include <CZ/Ream/GL/RGLDevice.h>
#include <format>

#include <iomanip>
#include <string>
#include <unordered_map>

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

#ifdef HAS_PIXEL_SIZE
    uniform float pixelSize;
#endif

#if FX == 2
    uniform vec4 colorMatrix[3];
#endif

#if FX != 0
    uniform vec4 uvClamp; // Valid area (min UV, max UV): taps beyond it are mirrored back

    vec3 tap(vec2 uv)
    {
        uv = uvClamp.zw - abs(uvClamp.zw - (uvClamp.xy + abs(uv - uvClamp.xy))); // Mirror at both edges
        return texture2D(image, clamp(uv, uvClamp.xy, uvClamp.zw)).xyz;          // Reach > area: clamp
    }
#endif

void main()
{

// No effect
#if FX == 0

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

#elif FX == 1 // VibrancyH

    #define HBLUR

    gl_FragColor.w = 1.0;

#else // VibrancyV

    #define VBLUR

    // Saturation, tint and brightness folded into one affine matrix (row-major 3x4)
    vec4 c = vec4(gl_FragColor.xyz, 1.0);
    gl_FragColor = vec4(clamp(vec3(dot(colorMatrix[0], c), dot(colorMatrix[1], c), dot(colorMatrix[2], c)), 0.0, 1.0), 1.0);

#endif
}
)";

static std::string GenHBlur(const std::vector<float> &kernel)
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(4); // Set precision for floats

    // Start with the initial gl_FragColor assignment
    oss << "gl_FragColor.xyz = " << kernel[0] << " * tap(imageCord);\n";

    // Loop over the remaining kernel elements
    for (size_t i = 1; i < kernel.size(); ++i) {
        float offset = static_cast<float>(i); // Offset for texSize.x
        oss << "gl_FragColor.xyz += " << kernel[i] << " * tap(imageCord + vec2("
            << offset << " * pixelSize, 0.0));\n";
        oss << "gl_FragColor.xyz += " << kernel[i] << " * tap(imageCord - vec2("
            << offset << " * pixelSize, 0.0));\n";
    }

    return oss.str();
}

static std::string GenVBlur(const std::vector<float> &kernel)
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(4); // Set precision for floats

    // Start with the initial gl_FragColor assignment
    oss << "gl_FragColor.xyz = " << kernel[0] << " * tap(imageCord);\n";

    // Loop over the remaining kernel elements
    for (size_t i = 1; i < kernel.size(); ++i) {
        float offset = static_cast<float>(i); // Offset for texSize.y
        oss << "gl_FragColor.xyz += " << kernel[i] << " * tap(imageCord + vec2(0.0, "
            << offset << " * pixelSize));\n";
        oss << "gl_FragColor.xyz += " << kernel[i] << " * tap(imageCord - vec2(0.0, "
            << offset << " * pixelSize));\n";
    }

    return oss.str();
}

std::shared_ptr<RGLShader> RGLShader::GetOrMake(RGLDevice *device, CZBitset<Features> features, GLenum type) noexcept
{
    auto current { RGLMakeCurrent::FromDevice(device, false) };
    auto *deviceData { (RGLDevice::ThreadData*)device->m_threadData->getData(device) };
    auto *map { &deviceData->fragShaders };

    if (type == GL_VERTEX_SHADER)
    {
        features = features.get() & VertFeatures;
        map = &deviceData->vertShaders;
    }

    auto it { map->find(features) };

    if (it != map->end())
        return it->second;

    auto shader { std::shared_ptr<RGLShader>(new RGLShader(device, features, type)) };

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

    // MakeCurrent by RGLDevice::ThreadData
    glDeleteShader(m_id);
}

static GLuint CompileShader(RGLDevice *device, GLenum type, const std::string &features, UInt32 fx)
{
    std::string source;

    if (type == GL_VERTEX_SHADER)
        source = features + v;
    else
    {
        source = features + f;

        if (fx == RGLShader::VibrancyH >> 28)
        {
            // Sigma 6
            const std::vector<float> kernelH { 0.0399,	0.0397,	0.0391,	0.0382,	0.0368,	0.0352,	0.0333,	0.0312,	0.0290,	0.0266,	0.0242,	0.0218,	0.0194,	0.0172,	0.0150,	0.0130,	0.0111 };
            const std::string placeholderH { "#define HBLUR" };
            source.replace(source.find(placeholderH), placeholderH.length(), GenHBlur(kernelH));
        }
        else if (fx == RGLShader::VibrancyV >> 28)
        {
            // Sigma 3
            const std::vector<float> kernelV { 0.0797,	0.0781,	0.0736,	0.0666,	0.0579,	0.0484,	0.0389,	0.0300,	0.0223,	0.0159,	0.0109,	0.0071,	0.0045,	0.0027 };
            const std::string placeholderV { "#define VBLUR" };
            source.replace(source.find(placeholderV), placeholderV.length(), GenVBlur(kernelV));
        }
    }

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
        device->log(CZError, "Failed to compile shader: {}\nSource: {}", (const char*)errorMsg.data(), source);
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

bool RGLShader::build() noexcept
{
    UInt32 fx { UInt32((m_features.get() & 0xF0000000) >> 28) };

    const std::string featuresStr {
        std::format("{}{}{}{}{}{}{}{}{}{}{}{}#define BLEND_MODE {}\n#define FX {}\n",
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
            !m_features.has(HasPixelSize)                 ? "" : "#define HAS_PIXEL_SIZE\n",
            UInt32(m_features.get() & 0x3),               // Blend Mode
            fx)                                           // Effect
    };

    m_id = CompileShader(device(), type(), featuresStr, fx);
    return m_id != 0;
}
