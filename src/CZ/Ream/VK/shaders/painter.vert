#version 450

// Positions are pre-transformed to Vulkan NDC on the CPU, and UVs to normalized image
// coordinates, so the vertex stage is a passthrough. This avoids GL/VK matrix-convention
// and clip-space (y-down, [0,1] depth) mismatches.
layout(location = 0) in vec2 inPos;
layout(location = 1) in vec2 inImageUV;
layout(location = 2) in vec2 inMaskUV;

layout(location = 0) out vec2 vImageUV;
layout(location = 1) out vec2 vMaskUV;

void main()
{
    gl_Position = vec4(inPos, 0.0, 1.0);
    vImageUV = inImageUV;
    vMaskUV = inMaskUV;
}
