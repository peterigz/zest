#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable

layout (set = 0, binding = 0) uniform sampler samplers[];
layout (set = 0, binding = 2) uniform textureCube textures[];

layout (location = 0) in vec3 inUVW;

layout (location = 0) out vec4 outColor;

layout (push_constant) uniform push {
	uint sampler_index;
	uint planet_index;
	uint rock_index;
	uint skybox_index;
	uint ubo_index;
} pc;

void main() 
{
	vec3 color = textureLod(samplerCube(textures[pc.skybox_index], samplers[pc.sampler_index]), inUVW, 0.0).rgb;

	outColor = vec4(color, 1.0);
}
