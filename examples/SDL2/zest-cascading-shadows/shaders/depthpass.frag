#version 450
#extension GL_EXT_nonuniform_qualifier : enable

layout (binding = 0) uniform sampler samplers[];
layout (binding = 1) uniform texture2D textures[];
layout (binding = 3) uniform texture2DArray textureArrays[];

layout(location = 0) in vec3 inUV;

layout(push_constant) uniform push {
	uint sampler_index;
	uint shadow_sampler_index;
	uint shadow_index;
	uint vert_index;
	uint frag_index;
	uint cascade_index;
	uint texture_index;
	uint display_cascade_index;
	int enable_pcf;
} pc;

void main() 
{	
	float alpha = texture(sampler2D(textures[pc.texture_index], samplers[pc.sampler_index]), inUV.xy).a;
	if (alpha < 0.5) {
		discard;
	}
}