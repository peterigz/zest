#version 450
#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 instance_position;
layout(location = 2) in vec4 instance_color;
layout(location = 3) in vec3 instance_rotation;
layout(location = 4) in vec3 instance_scale;

layout (binding = 7) uniform UBO 
{
	mat4 depthMVP;
} ubo[];

layout(push_constant) uniform push
{
	uint sampler_index;
	uint shadow_index;
	uint view_uniform_index;
	uint offscreen_uniform_index;
	bool enable_pcf;
} pc;

out gl_PerVertex 
{
    vec4 gl_Position;   
};

 
void main()
{
	gl_Position =  ubo[pc.offscreen_uniform_index].depthMVP * vec4(inPos, 1.0);
}