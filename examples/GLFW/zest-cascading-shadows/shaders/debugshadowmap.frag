#version 450
#extension GL_EXT_nonuniform_qualifier : enable

layout (binding = 0) uniform sampler samplers[];
layout (binding = 3) uniform texture2DArray textureArrays[];

layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 outFragColor;

layout(push_constant) uniform push {
	uint sampler_index;
	uint shadow_sampler_index;
	uint shadow_index;
	uint vert_index;
	uint frag_index;
	uint cascade_index;
	uint texture_index;
	uint display_cascade_index;
	int enable_pcf;
} pc;

void main() 
{
	float depth = texture(sampler2DArray(textureArrays[pc.shadow_index], samplers[pc.shadow_sampler_index]), vec3(inUV, float(pc.display_cascade_index))).r;
	outFragColor = vec4(vec3((depth)), 1.0);
}