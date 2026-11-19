#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : require

layout (binding = 7) uniform ubo_view 
{
	mat4 view;
	mat4 proj;
} uboView[];

//The push constant struct for storing 
layout(push_constant) uniform push_constants
{
	uint sampler_index;
	uint texture_index;
	uint uniform_index;
} pc;

layout(location = 0) in vec4 vertex_position;
layout(location = 1) in vec4 uv;
layout(location = 2) in vec4 color;

layout(location = 0) out vec4 out_uv;
layout(location = 1) out vec4 out_color;

void main() {
	gl_Position = (uboView[pc.uniform_index].proj * uboView[pc.uniform_index].view * vec4(vertex_position.xyz, 1.0));
	out_uv = uv;
	out_color = color;
}