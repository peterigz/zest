#version 450

layout (binding = 0) uniform sampler2D samplerColorMap[];
layout (binding = 0) uniform sampler2D samplerGradientRamp[];

layout (location = 0) in vec4 inColor;
layout (location = 1) in float inGradientPos;

layout (location = 0) out vec4 outFragColor;

void main () 
{
	vec3 color = texture(samplerGradientRamp[1], vec2(inGradientPos, 0.0)).rgb;
	outFragColor.rgb = texture(samplerColorMap[0], gl_PointCoord).rgb * color;
}
