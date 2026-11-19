#version 450
#extension GL_EXT_nonuniform_qualifier : require

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec4 inColor;
layout (location = 2) in vec3 inNormal;
layout (location = 3) in vec2 inUV;

layout (binding = 7) uniform UBO 
{
	mat4 projection;
	mat4 modelview;
	vec4 lightPos;
} ubo[];

layout (push_constant) uniform push {
	uint sampler_index;
	uint planet_index;
	uint rock_index;
	uint skybox_index;
	uint ubo_index;
} pc;

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec3 outColor;
layout (location = 2) out vec2 outUV;
layout (location = 3) out vec3 outViewVec;
layout (location = 4) out vec3 outLightVec;

void main() 
{
	outColor = inColor.rgb;
	outUV = inUV;
	gl_Position = ubo[pc.ubo_index].projection * ubo[pc.ubo_index].modelview * vec4(inPos.xyz, 1.0);
	
	vec4 pos = ubo[pc.ubo_index].modelview * vec4(inPos, 1.0);
	outNormal = mat3(ubo[pc.ubo_index].modelview) * inNormal;
	vec3 lPos = mat3(ubo[pc.ubo_index].modelview) * ubo[pc.ubo_index].lightPos.xyz;
	outLightVec = lPos - pos.xyz;
	outViewVec = -pos.xyz;		
}