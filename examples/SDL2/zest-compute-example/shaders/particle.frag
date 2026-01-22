#version 450
#extension GL_EXT_nonuniform_qualifier : require

layout (binding = 0) uniform sampler2D samplers[];

layout (location = 0) in vec4 inColor;
layout (location = 1) in float inGradientPos;

layout (location = 0) out vec4 outFragColor;

layout(push_constant) uniform push_constants
{
	int particle_index;
	int gradient_index;
} pc;


void main () 
{
	vec3 color = texture(samplers[pc.gradient_index], vec2(inGradientPos, 0.0)).rgb;
	outFragColor.rgb = texture(samplers[pc.particle_index], gl_PointCoord).rgb * color;
}
