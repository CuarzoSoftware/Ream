#version 450

// drawColor: outputs a premultiplied color. Blend mode (Src / SrcOver / DstIn) is handled by
// fixed-function blend state, matching the GL painter's glBlendFunc choices.
layout(location = 0) in vec2 vImageUV;
layout(location = 1) in vec2 vMaskUV;

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants {
    vec4 color;   // premultiplied
    vec4 factor;  // unused by color mode
} pc;

void main()
{
    outColor = pc.color;
}
