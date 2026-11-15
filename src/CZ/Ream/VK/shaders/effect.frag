#version 450

// drawImageEffect: Gaussian blur passes, ported from the GL uber-shader's vibrancy effects. FX
// selects the pass (1=horizontal, 2=vertical + color matrix). pixelSize (push constant factor.x)
// is the per-tap step in UV units.
layout(location = 0) in vec2 vImageUV;
layout(location = 1) in vec2 vMaskUV;

layout(location = 0) out vec4 outColor;

layout(constant_id = 0) const int FX = 1;

layout(set = 0, binding = 0) uniform sampler2D imageTex;
layout(set = 0, binding = 1) uniform sampler2D maskTex; // unused

layout(push_constant) uniform PushConstants {
    vec4 color;
    vec4 factor; // factor.x = pixelSize
    vec4 colorMatrix[3]; // FX 2: row-major 3x4 (saturation, tint and brightness folded in)
    vec4 uvClamp;        // Valid area (min UV, max UV): taps beyond it are mirrored back
} pc;

vec3 tap(vec2 uv)
{
    const vec2 lo = pc.uvClamp.xy;
    const vec2 hi = pc.uvClamp.zw;
    uv = hi - abs(hi - (lo + abs(uv - lo)));             // Mirror at both edges
    return texture(imageTex, clamp(uv, lo, hi)).xyz;    // Reach > area: clamp
}

const float kH[17] = float[](0.0399, 0.0397, 0.0391, 0.0382, 0.0368, 0.0352, 0.0333, 0.0312,
                             0.0290, 0.0266, 0.0242, 0.0218, 0.0194, 0.0172, 0.0150, 0.0130, 0.0111);
const float kV[14] = float[](0.0797, 0.0781, 0.0736, 0.0666, 0.0579, 0.0484, 0.0389, 0.0300,
                             0.0223, 0.0159, 0.0109, 0.0071, 0.0045, 0.0027);

void main()
{
    const float ps = pc.factor.x;

    if (FX == 1) // horizontal blur
    {
        vec3 c = kH[0] * tap(vImageUV);
        for (int i = 1; i < 17; i++)
        {
            c += kH[i] * tap(vImageUV + vec2(float(i) * ps, 0.0));
            c += kH[i] * tap(vImageUV - vec2(float(i) * ps, 0.0));
        }
        outColor = vec4(c, 1.0);
        return;
    }

    // vertical blur
    vec3 c = kV[0] * tap(vImageUV);
    for (int i = 1; i < 14; i++)
    {
        c += kV[i] * tap(vImageUV + vec2(0.0, float(i) * ps));
        c += kV[i] * tap(vImageUV - vec2(0.0, float(i) * ps));
    }

    const vec4 c4 = vec4(c, 1.0);
    c = vec3(dot(pc.colorMatrix[0], c4), dot(pc.colorMatrix[1], c4), dot(pc.colorMatrix[2], c4));
    outColor = vec4(clamp(c, 0.0, 1.0), 1.0);
}
