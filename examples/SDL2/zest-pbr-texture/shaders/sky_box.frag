#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable

layout (set = 0, binding = 0) uniform sampler samplers[];
layout (set = 0, binding = 2) uniform textureCube textures[];

layout (location = 0) in vec3 inUVW;

layout (location = 0) out vec4 outColor;

layout (binding = 7) uniform UBOParams {
	vec4 lights[4];
	float exposure;
	float gamma;
	uint texture_index;
	uint sampler_index;
} consts[];

layout(push_constant) uniform quad_index
{
	uint view_index;
	uint lights_index;
} pc;

// From http://filmicworlds.com/blog/filmic-tonemapping-operators/
vec3 Uncharted2Tonemap(vec3 color)
{
	float A = 0.15;
	float B = 0.50;
	float C = 0.10;
	float D = 0.20;
	float E = 0.02;
	float F = 0.30;
	float W = 11.2;
	return ((color*(A*color+C*B)+D*E)/(color*(A*color+B)+D*F))-E/F;
}

void main() 
{
	vec3 color = textureLod(samplerCube(textures[consts[pc.lights_index].texture_index], samplers[consts[pc.lights_index].sampler_index]), inUVW, 0.0).rgb;

	// Tone mapping
	color = Uncharted2Tonemap(color * consts[pc.lights_index].exposure);
	color = color * (1.0f / Uncharted2Tonemap(vec3(11.2f)));	
	// Gamma correction
	color = pow(color, vec3(1.0f / consts[pc.lights_index].gamma));
	
	outColor = vec4(color, 1.0);
}
