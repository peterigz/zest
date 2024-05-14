#version 450

layout(location = 0) in vec4 in_frag_color;

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform quad_index
{
    mat4 model;
    vec4 parameters1;
	vec4 parameters2;
	vec4 parameters3;
	vec4 camera;
} pc;

void main() {
	outColor = in_frag_color;
}
