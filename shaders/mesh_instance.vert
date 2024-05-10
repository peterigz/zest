#version 450
#extension GL_ARB_separate_shader_objects : enable

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
} pc;

layout(location = 0) in vec3 vertex_position;
layout(location = 1) in vec4 vertex_color;
layout(location = 2) in vec3 vertex_normal;
layout(location = 3) in vec3 instance_position;
layout(location = 4) in vec4 instance_color;
layout(location = 5) in vec3 instance_rotation;
layout(location = 6) in vec4 instance_parameters;
layout(location = 7) in vec3 instance_scale;

layout(location = 0) out vec4 out_frag_color;

void main() {
	mat3 mx, my, mz;
	
	// rotate around x
	float s = sin(instance_rotation.x);
	float c = cos(instance_rotation.x);

	mx[0] = vec3(c, s, 0.0);
	mx[1] = vec3(-s, c, 0.0);
	mx[2] = vec3(0.0, 0.0, 1.0);
	
	// rotate around y
	s = sin(instance_rotation.y);
	c = cos(instance_rotation.y);

	my[0] = vec3(c, 0.0, s);
	my[1] = vec3(0.0, 1.0, 0.0);
	my[2] = vec3(-s, 0.0, c);
	
	// rot around z
	s = sin(instance_rotation.z);
	c = cos(instance_rotation.z);
	
	mz[0] = vec3(1.0, 0.0, 0.0);
	mz[1] = vec3(0.0, c, s);
	mz[2] = vec3(0.0, -s, c);
	
	mat3 rotation_matrix = mz * my * mx;
	vec3 position = vertex_position * instance_scale * rotation_matrix + instance_position;

	/*
	mat3 rotation_matrix = mz * my * mx;
	vec3 position = vertex_position * instance_scale * rotation_matrix + instance_position;
	position.y *= -1;

	vec4 clip = uboView.proj * uboView.view * vec4(instance_position, 1.0);
	vec2 screen = uboView.res * (0.5 * clip.xy / clip.w + 0.5);

	vec2 point = screen + position.xy * 25;
	gl_Position = vec4(clip.w * ((2.0 * point) / uboView.res - 1.0), clip.z, clip.w);
	*/

	gl_Position = (uboView.proj * uboView.view * pc.model * vec4(position, 1.0));

	out_frag_color = vertex_color * instance_color;
}