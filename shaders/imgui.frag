#version 450
#extension GL_ARB_separate_shader_objects : enable


layout (location = 0) in vec4 inColor;
layout (location = 1) in vec3 inUV;

layout(location = 0) out vec4 outColor1;

layout(binding = 1) uniform sampler2DArray texSampler;

layout(push_constant) uniform quad_index
{
    mat4 model;
    vec4 transform;
	vec4 parameters;
	uint flags;
} pushConstants;

void main() 
{
	vec4 Tex_Color = texture(texSampler, inUV);

	if(pushConstants.flags == 1) {
		//Pass through blend
		Tex_Color.a = 1.f;
	} else if(pushConstants.flags == 2 && Tex_Color.a > 0) {
		//PreMultiply blend
		Tex_Color.rgb /= Tex_Color.a;
	}

	outColor1 = inColor * Tex_Color;

}