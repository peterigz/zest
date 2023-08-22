#version 450
#extension GL_ARB_separate_shader_objects : enable

//Blendmodes
const uint BLENDMODE_NONE = 0;
const uint BLENDMODE_ALPHA = 1;
const uint BLENDMODE_ADDITIVE = 2;
const uint BLENDMODE_X2 = 3;

//Not actually used with imgui
layout (binding = 0) uniform UboView 
{
	mat4 view;
	mat4 proj;
	vec4 parameters1;
	vec4 parameters2;
	vec2 res;
	uint millisecs;
} uboView;

layout(push_constant) uniform quad_index
{
    mat4 model;
    vec4 transform;
	vec4 parameters;
	vec4 parameters3;
	vec4 camera;
	uint flags;
} pc;

layout(location = 0) in vec2 in_position;
layout(location = 1) in vec2 in_tex_coord;
layout(location = 2) in vec4 in_color;

layout(location = 0) out vec4 out_color;
layout(location = 1) out vec3 out_uv;

void main() {

	gl_Position = vec4(in_position * pc.transform.xy + pc.transform.zw, 0.0, 1.0);

	out_uv = vec3(in_tex_coord.xy, pc.parameters.x);
	out_color = in_color;
}
