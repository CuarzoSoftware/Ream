#version 450

// drawImageEffect: Gaussian blur passes with optional YUV saturation, ported from the GL
// uber-shader's vibrancy effects. FX selects the pass (1=horizontal, 2=vertical light,
// 3=vertical dark). pixelSize (push constant factor.x) is the per-tap texel step.
layout(location = 0) in vec2 vImageUV;
layout(location = 1) in vec2 vMaskUV;

layout(location = 0) out vec4 outColor;

layout(constant_id = 0) const int FX = 1;

layout(set = 0, binding = 0) uniform sampler2D imageTex;
layout(set = 0, binding = 1) uniform sampler2D maskTex; // unused

layout(push_constant) uniform PushConstants {
    vec4 color;
    vec4 factor; // factor.x = pixelSize
} pc;

const float kH[17] = float[](0.0399, 0.0397, 0.0391, 0.0382, 0.0368, 0.0352, 0.0333, 0.0312,
                             0.0290, 0.0266, 0.0242, 0.0218, 0.0194, 0.0172, 0.0150, 0.0130, 0.0111);
const float kV[14] = float[](0.0797, 0.0781, 0.0736, 0.0666, 0.0579, 0.0484, 0.0389, 0.0300,
                             0.0223, 0.0159, 0.0109, 0.0071, 0.0045, 0.0027);

void main()
{
    const float ps = pc.factor.x;

    if (FX == 1) // horizontal blur
    {
        vec3 c = kH[0] * texture(imageTex, vImageUV).xyz;
        for (int i = 1; i < 17; i++)
        {
            c += kH[i] * texture(imageTex, vImageUV + vec2(float(i) * ps, 0.0)).xyz;
            c += kH[i] * texture(imageTex, vImageUV - vec2(float(i) * ps, 0.0)).xyz;
        }
        outColor = vec4(c, 1.0);
        return;
    }

    // vertical blur
    vec3 c = kV[0] * texture(imageTex, vImageUV).xyz;
    for (int i = 1; i < 14; i++)
    {
        c += kV[i] * texture(imageTex, vImageUV + vec2(0.0, float(i) * ps)).xyz;
        c += kV[i] * texture(imageTex, vImageUV - vec2(0.0, float(i) * ps)).xyz;
    }

    // YUV saturation (matrices are column-major literals matching the GL shader; c * M is a
    // row-vector multiply in GLSL, identical to the GL uber-shader).
    const mat3 rgbToYuv = mat3(0.299, 0.587, 0.114,
                              -0.14713, -0.28886, 0.436,
                               0.615, -0.51498, -0.10001);
    const mat3 yuvToRgb = mat3(1.0, 0.0, 1.13983,
                               1.0, -0.39465, -0.58060,
                               1.0, 2.03211, 0.0);
    c = c * rgbToYuv;
    c.y *= 24.0; c.z *= 24.0; c.x *= 12.0;
    c = c * yuvToRgb;

    if (FX == 2) // light
        c = (0.1 / 6.0) * min(c, vec3(7.0)) + 0.78;
    else         // dark
        c = (0.2 / 6.0) * min(c, vec3(3.0)) + 0.1;

    outColor = vec4(c, 1.0);
}
