#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 1, binding = 0) uniform UboView
{
    mat4 view;
    mat4 proj;
    vec2 screen_size;
    float timer_lerp;
    float update_time;
} uboView;

layout(push_constant) uniform quad_index
{
	vec4 camera;
	vec3 color;
	float roughness;
	float metallic;
	uint irradiance_index;
	uint brd_lookup_index;
	uint pre_filtered_index;
	uint sampler_index;
	uint skybox_sampler_index;
};

layout(location = 0) in vec3 vertex_position;
layout(location = 1) in vec4 vertex_color;
layout(location = 2) in uint group_id;
layout(location = 3) in vec3 vertex_normal;
layout(location = 4) in vec3 instance_position;
layout(location = 5) in vec4 instance_color;
layout(location = 6) in vec3 instance_rotation;
layout(location = 7) in vec4 instance_parameters;
layout(location = 8) in vec3 instance_scale;

layout(location = 0) out vec4 out_frag_color;
layout (location = 1) out vec3 out_world_position;
layout (location = 2) out vec3 out_normal;

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
	gl_Position = (uboView.proj * uboView.view * vec4(position, 1.0));

	vec4 color = vertex_color * instance_color;
	
	out_frag_color = color;
	out_world_position = position;
	out_normal = vertex_normal * rotation_matrix;
}