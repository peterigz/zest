#version 450
#extension GL_EXT_nonuniform_qualifier : require

layout(set = 0, binding = 0) uniform sampler2D samplerColor[];
layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outFragColor;

layout(push_constant) uniform texture_indexes
{
    uint index1;
} pc;

void main(void)
{
    outFragColor = texture(samplerColor[pc.index1], inUV);
}
