#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec3 vertex_position;
layout(location = 1) in vec4 vertex_color;
layout(location = 2) in vec3 vertex_normal;
layout(location = 3) in vec2 in_uv;
layout(location = 4) in vec4 tangent;
layout(location = 5) in uint group_id;
layout(location = 6) in vec3 instance_position;
layout(location = 7) in vec4 instance_color;
layout(location = 8) in vec3 instance_rotation;
layout(location = 9) in float rougness;
layout(location = 10) in vec3 instance_scale;
layout(location = 11) in float metallic;

layout(binding = 7) uniform UboView
{
    mat4 view;
    mat4 proj;
    vec2 screen_size;
    float timer_lerp;
    float update_time;
} ubo[];

layout(push_constant) uniform quad_index
{
	uint view_index;
	uint lights_index;
} pc;

layout (location = 0) out vec3 out_uvw;

void main() 
{
	out_uvw = vertex_position;
    out_uvw.y *= -1;
	mat4 viewNoTranslation = mat4(mat3(ubo[pc.view_index].view));
	vec4 clip_pos = ubo[pc.view_index].proj * viewNoTranslation * vec4(vertex_position.xyz, 1.0);
    gl_Position = clip_pos.xyww;
}