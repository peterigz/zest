#version 450
#extension GL_ARB_separate_shader_objects : enable


layout (location = 0) in vec4 in_color;
layout (location = 1) in vec3 in_uv;

layout(location = 0) out vec4 out_color;

layout(binding = 1) uniform sampler2DArray tex_sampler;

layout(push_constant) uniform quad_index
{
    mat4 model;
    vec4 parameters1;
	vec4 parameters2;
	vec4 parameters3;
	vec4 camera;
	uint flags;
} pc;

void main() 
{
	vec4 tex_color = texture(tex_sampler, in_uv);

	if(pc.flags == 1) {
		//Pass through blend
		tex_color.a = 1.f;
	} else if(pc.flags == 2 && tex_color.a > 0) {
		//PreMultiply blend
		tex_color.rgb /= tex_color.a;
	}

	out_color = in_color * tex_color;
}