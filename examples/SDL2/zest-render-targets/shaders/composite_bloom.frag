#version 450
#extension GL_EXT_nonuniform_qualifier : require

layout(set = 0, binding = 0) uniform sampler u_samplers[];
layout(set = 0, binding = 1) uniform texture2D u_textures[];

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outFragColor;

layout(push_constant) uniform texture_indexes
{
    uint base_index;
    uint bloom_index;
	uint sampler_index;
    float bloom_alpha;
} pc;

void main(void)
{
	vec4 base_color = texture(sampler2D(u_textures[pc.base_index], u_samplers[pc.sampler_index]), inUV);
	vec4 bloom_color = texture(sampler2D(u_textures[pc.bloom_index], u_samplers[pc.sampler_index]), inUV) * pc.bloom_alpha;
    outFragColor = base_color + bloom_color;
}
