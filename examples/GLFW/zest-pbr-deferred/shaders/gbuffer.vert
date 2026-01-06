#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : require

layout(binding = 7) uniform UboView
{
    mat4 view;
    mat4 proj;
    vec2 screen_size;
    float timer_lerp;
    float update_time;
} uboView[];

layout(push_constant) uniform gbuffer_pc
{
	vec3 color;
	uint _padding;
	uint view_index;
} pc;

layout(location = 0) in vec3 vertex_position;
layout(location = 1) in vec4 vertex_color;
layout(location = 2) in vec3 vertex_normal;
layout(location = 3) in vec2 in_uv;
layout(location = 4) in vec4 tangent;
layout(location = 5) in uint group_id;
layout(location = 6) in vec3 instance_position;
layout(location = 7) in vec4 instance_color;
layout(location = 8) in vec3 instance_rotation;
layout(location = 9) in float roughness;
layout(location = 10) in vec3 instance_scale;
layout(location = 11) in float metallic;

layout(location = 0) out vec3 out_world_position;
layout(location = 1) out vec3 out_normal;
layout(location = 2) out vec4 out_color;
layout(location = 3) out vec2 out_pbr;

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
	gl_Position = (uboView[pc.view_index].proj * uboView[pc.view_index].view * vec4(position, 1.0));

	vec4 color = vertex_color * instance_color;

	out_world_position = position;
	out_normal = vertex_normal * rotation_matrix;
	out_color = vec4(pc.color * color.rgb, 1.0);
	out_pbr = vec2(roughness, metallic);
}
