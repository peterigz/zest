#version 450
#extension GL_ARB_separate_shader_objects : enable

//Blendmodes
const uint BLENDMODE_NONE = 0;
const uint BLENDMODE_ALPHA = 1;
const uint BLENDMODE_ADDITIVE = 2;

layout (binding = 0) uniform ubo_view 
{
	mat4 view;
	mat4 proj;
	vec4 parameters1;
	vec4 parameters2;
	vec2 res;
	uint millisecs;
} uboView;

layout(push_constant) uniform parameters
{
    mat4 model;
    vec4 parameters1;
	vec4 parameters2;
	vec4 parameters3;
	vec4 camera;
	uint flags;
} pc;

layout(location = 0) in vec3 in_position;
layout(location = 1) in float in_intensity;
layout(location = 2) in vec2 in_tex_coord;
layout(location = 3) in vec4 in_color;
layout(location = 4) in uint in_texture_array_index;

layout(location = 0) out vec4 out_frag_color;
layout(location = 1) out vec3 out_tex_coord;

void main() {
	gl_Position = uboView.proj * uboView.view * pc.model * vec4(in_position.xyz, 1.0);

	out_tex_coord = vec3(in_tex_coord, in_texture_array_index);

	out_frag_color = in_color * in_intensity;
}