#version 450
#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec3 vertex_position;
layout(location = 1) in vec4 vertex_color;
layout(location = 2) in vec3 vertex_normal;
layout(location = 3) in vec2 in_uv;
layout(location = 4) in vec3 instance_position;
layout(location = 5) in vec4 instance_color;
layout(location = 6) in vec3 instance_rotation;
layout(location = 7) in vec3 instance_scale;

#define SHADOW_MAP_CASCADE_COUNT 4

layout (binding = 7) uniform UBO {
	mat4 projection;
	mat4 view;
	mat4 model;
} ubo[];

layout(push_constant) uniform push {
	uint sampler_index;
	uint shadow_sampler_index;
	uint shadow_index;
	uint vert_index;
	uint frag_index;
	uint cascade_index;
	uint texture_index;
	uint display_cascade_index;
	bool enable_pcf;
} pc;

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec3 outColor;
layout (location = 2) out vec3 outViewPos;
layout (location = 3) out vec3 outPos;
layout (location = 4) out vec3 outUV;

void main()
{
	outColor = vertex_color.rgb * instance_color.rgb;

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
	vec3 pos = vertex_position * instance_scale * rotation_matrix + instance_position;

	outColor = vertex_color.rgb;
	outNormal = mat3(ubo[pc.vert_index].model) * (rotation_matrix * vertex_normal);
	outUV = vec3(in_uv, 0);
	outPos = pos;
	outViewPos = (ubo[pc.vert_index].view * vec4(pos.xyz, 1.0)).xyz;
	gl_Position = ubo[pc.vert_index].projection * ubo[pc.vert_index].view * ubo[pc.vert_index].model * vec4(pos.xyz, 1.0);
}
