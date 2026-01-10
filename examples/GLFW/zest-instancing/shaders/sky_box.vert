#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec3 vertex_position;

layout (binding = 7) uniform UBO 
{
	mat4 proj;
	mat4 view;
	vec4 lightPos;
} ubo[];

layout (push_constant) uniform push {
	uint sampler_index;
	uint planet_index;
	uint rock_index;
	uint skybox_index;
	uint ubo_index;
} pc;

layout (location = 0) out vec3 out_uvw;

void main() 
{
	out_uvw = vertex_position;
    out_uvw.y *= -1;
	mat4 viewNoTranslation = mat4(mat3(ubo[pc.ubo_index].view));
	vec4 clip_pos = ubo[pc.ubo_index].proj * viewNoTranslation * vec4(vertex_position.xyz, 1.0);
    gl_Position = clip_pos.xyww;
}