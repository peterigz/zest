#version 450

layout (binding = 1) uniform sampler2DArray samplerColor;

layout (location = 0) in vec2 inUV;
layout (location = 1) in vec2 inRes;
layout (location = 0) out vec4 outFragColor;

void main(void)
{
	outFragColor = texture(samplerColor, vec3(inUV, 0));
	//if(pushConstants.flags == 2 && Tex_Color.a > 0) {
		//PreMultiply blend
		//Tex_Color.rgb /= Tex_Color.a;
	//}

}

