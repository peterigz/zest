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

layout (binding = 7) uniform UBO 
{
	mat4 projection;
	mat4 view;
	mat4 model;
	mat4 lightSpace;
	vec4 lightPos;
	float zNear;
	float zFar;
} ubo[];

layout(push_constant) uniform indexes {
	uint sampler_index;
	uint shadow_index;
	uint uniform_index;
	uint offscreen_uniform_index;
	bool enable_pcf;
} pc;

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec3 outColor;
layout (location = 2) out vec3 outViewVec;
layout (location = 3) out vec3 outLightVec;
layout (location = 4) out vec4 outShadowCoord;

const mat4 biasMat = mat4( 
	0.5, 0.0, 0.0, 0.0,
	0.0, 0.5, 0.0, 0.0,
	0.0, 0.0, 1.0, 0.0,
	0.5, 0.5, 0.0, 1.0 );

void main() 
{
	outColor = vertex_color.rgb * instance_color.rgb;
	outNormal = vertex_normal;

	gl_Position = ubo[pc.uniform_index].projection * ubo[pc.uniform_index].view * ubo[pc.uniform_index].model * vec4(vertex_position.xyz, 1.0);
	
    vec4 pos = ubo[pc.uniform_index].model * vec4(vertex_position, 1.0);
    outNormal = mat3(ubo[pc.uniform_index].model) * vertex_normal;
    outLightVec = normalize(ubo[pc.uniform_index].lightPos.xyz - vertex_position);
    outViewVec = -pos.xyz;			

	outShadowCoord = ( biasMat * ubo[pc.uniform_index].lightSpace * ubo[pc.uniform_index].model ) * vec4(vertex_position, 1.0);	
}

