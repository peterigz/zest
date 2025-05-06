#version 450

layout(location = 0) out vec2 outUV;
layout(location = 1) out uint blendmode;
layout(location = 2) out float intensity;

layout(push_constant) uniform pushes
{
    vec2 res;
    uint flags;
    float alpha_level;
} pc;

out gl_PerVertex
{
    vec4 gl_Position;
};

void main()
{
    outUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(outUV * 2.0f - 1.0f, 0.0f, 1.0f);
    blendmode = pc.flags;
    intensity = pc.alpha_level;
}