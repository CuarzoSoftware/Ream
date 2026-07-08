#version 450

// drawImage. Feature toggles are specialization constants (the SPIR-V analog of the GL
// uber-shader's #defines); blend mode is fixed-function state chosen by RVKPainter to match
// the GL glBlendFunc selection. Single-plane RGBA dma-buf images sample as regular sampler2D
// (no external sampler needed on Vulkan).
layout(location = 0) in vec2 vImageUV;
layout(location = 1) in vec2 vMaskUV;

layout(location = 0) out vec4 outColor;

layout(constant_id = 0) const int HAS_MASK = 0;
layout(constant_id = 1) const int REPLACE_IMAGE_COLOR = 0;
layout(constant_id = 2) const int PREMULT_SRC = 0;
layout(constant_id = 3) const int BLEND_DSTIN = 0;

layout(set = 0, binding = 0) uniform sampler2D imageTex;
layout(set = 0, binding = 1) uniform sampler2D maskTex;

layout(push_constant) uniform PushConstants {
    vec4 color;   // unused by image mode
    vec4 factor;  // rgb: per-channel factor, a: final alpha (opacity * factor.a)
} pc;

void main()
{
    if (BLEND_DSTIN != 0)
    {
        // Only alpha matters; RGB is discarded by the ZERO,SRC_ALPHA blend.
        float a = texture(imageTex, vImageUV).a;
        if (HAS_MASK != 0)
            a *= texture(maskTex, vMaskUV).a;
        a *= pc.factor.a;
        outColor = vec4(0.0, 0.0, 0.0, a);
        return;
    }

    if (REPLACE_IMAGE_COLOR != 0)
    {
        // RGB is the (unpremultiplied) factor; blend state premultiplies by src alpha.
        float a = texture(imageTex, vImageUV).a;
        if (HAS_MASK != 0)
            a *= texture(maskTex, vMaskUV).a;
        a *= pc.factor.a;
        outColor = vec4(pc.factor.rgb, a);
        return;
    }

    vec4 c = texture(imageTex, vImageUV);
    c.rgb *= pc.factor.rgb;

    if (PREMULT_SRC != 0)
    {
        float m = pc.factor.a;
        if (HAS_MASK != 0)
            m *= texture(maskTex, vMaskUV).a;
        c *= m;
    }
    else
    {
        c.a *= pc.factor.a;
        if (HAS_MASK != 0)
            c.a *= texture(maskTex, vMaskUV).a;
    }

    outColor = c;
}
