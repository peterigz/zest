#version 450
#extension GL_ARB_separate_shader_objects : enable

//Blendmodes
const uint BLENDMODE_NONE = 0;
const uint BLENDMODE_ALPHA = 1;
const uint BLENDMODE_ADDITIVE = 2;
const uint BLENDMODE_X2 = 3;

//Not actually used with imgui
layout (binding = 0) uniform UboView 
{
	mat4 scale;
	mat4 translate;	
	vec4 parameters1;
	vec4 parameters2;
	vec2 res;
	uint millisecs;
} uboView;

layout(push_constant) uniform quad_index
{
    mat4 model;
	mat4 view;
	mat4 proj;
    vec4 transform;
	vec4 parameters;
	uint flags;
} pushConstants;

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec4 inColor;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec3 fragTexCoord;

void main() {

	gl_Position = vec4(inPosition * pushConstants.transform.xy + pushConstants.transform.zw, 0.0, 1.0);

	fragTexCoord = vec3(inTexCoord.xy, pushConstants.parameters.x);
	//float Gamma = 2.2f;
    //float LinearAlpha = pow(inColor.a, 1.0 / Gamma);
    //fragColor = vec4(pow(inColor.rgb * vec3(LinearAlpha), vec3(Gamma)), inColor.a);
	fragColor = inColor;
}
