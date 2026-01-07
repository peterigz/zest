#version 450
#extension GL_EXT_nonuniform_qualifier : enable

layout (set = 0, binding = 0) uniform sampler samplers[];
layout (set = 0, binding = 1) uniform texture2D textures_2d[];

layout (location = 0) in vec2 inUV;
layout (location = 0) out vec4 outFragColor;

layout(push_constant) uniform push_consts
{
	uint sampler_index;
	uint gTarget;
} pc;

void main(void)
{
	outFragColor = texture(sampler2D(textures_2d[pc.gTarget], samplers[pc.sampler_index]), inUV);
}

