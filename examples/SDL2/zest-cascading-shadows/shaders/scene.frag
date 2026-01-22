#version 450
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_samplerless_texture_functions : enable

#define SHADOW_MAP_CASCADE_COUNT 4

layout (binding = 0) uniform sampler samplers[];
layout (binding = 1) uniform texture2D textures[];
layout (binding = 3) uniform texture2DArray textureArrays[];

layout (location = 0) out vec4 outFragColor;

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec3 inViewPos;
layout (location = 3) in vec3 inPos;
layout (location = 4) in vec3 inUV;

#define ambient 0.3

layout (binding = 7) uniform UBO {
	vec4 cascadeSplits;
	mat4 inverseViewMat;
	vec3 lightDir;
	float _pad;
	int colorCascades;
} ubo[];

layout (binding = 7) uniform CVPM {
	mat4 matrices[SHADOW_MAP_CASCADE_COUNT];
} cascadeViewProjMatrices[];

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

const mat4 biasMat = mat4( 
	0.5, 0.0, 0.0, 0.0,
	0.0, 0.5, 0.0, 0.0,
	0.0, 0.0, 1.0, 0.0,
	0.5, 0.5, 0.0, 1.0 
);

float textureProj(vec4 shadowCoord, vec2 offset, uint cascadeIndex)
{
	float shadow = 1.0;
	float bias = 0.005;

	if ( shadowCoord.z > -1.0 && shadowCoord.z < 1.0 ) {
		float dist = texture(sampler2DArray(textureArrays[pc.shadow_index], samplers[pc.shadow_sampler_index]), vec3(shadowCoord.st + offset, cascadeIndex)).r;
		if (shadowCoord.w > 0 && dist < shadowCoord.z - bias) {
			shadow = ambient;
		}
	}
	return shadow;

}

float filterPCF(vec4 sc, uint cascadeIndex)
{
	ivec2 texDim = textureSize(textureArrays[pc.shadow_index], 0).xy;
	float scale = 0.75;
	float dx = scale * 1.0 / float(texDim.x);
	float dy = scale * 1.0 / float(texDim.y);

	float shadowFactor = 0.0;
	int count = 0;
	int range = 1;
	
	for (int x = -range; x <= range; x++) {
		for (int y = -range; y <= range; y++) {
			shadowFactor += textureProj(sc, vec2(dx*x, dy*y), cascadeIndex);
			count++;
		}
	}
	return shadowFactor / count;
}

void main() 
{	
	vec4 color = texture(sampler2DArray(textureArrays[pc.texture_index], samplers[pc.sampler_index]), inUV);
	if (color.a < 0.5) {
		discard;
	}

	// Get cascade index for the current fragment's view position
	uint cascadeIndex = 0;
	for(uint i = 0; i < SHADOW_MAP_CASCADE_COUNT - 1; ++i) {
		if(inViewPos.z < ubo[pc.frag_index].cascadeSplits[i]) {	
			cascadeIndex = i + 1;
		}
	}

	// Depth compare for shadowing
	vec4 shadowCoord = (biasMat * cascadeViewProjMatrices[pc.cascade_index].matrices[cascadeIndex]) * vec4(inPos, 1.0);	

	float shadow = 0;
	if (pc.enable_pcf == 1) {
		shadow = filterPCF(shadowCoord / shadowCoord.w, cascadeIndex);
	} else {
		shadow = textureProj(shadowCoord / shadowCoord.w, vec2(0.0), cascadeIndex);
	}

	// Directional light
	vec3 N = normalize(inNormal);
	vec3 L = normalize(-ubo[pc.frag_index].lightDir);
	vec3 H = normalize(L + inViewPos);
	float diffuse = max(dot(N, L), ambient);
	vec3 lightColor = vec3(1.0);
	outFragColor.rgb = max(lightColor * (diffuse * color.rgb), vec3(0.0));
	outFragColor.rgb *= shadow;
	outFragColor.a = color.a;

	// Color cascades (if enabled)
	if (ubo[pc.frag_index].colorCascades == 1) {
		switch(cascadeIndex) {
			case 0 : 
				outFragColor.rgb *= vec3(1.0f, 0.25f, 0.25f);
				break;
			case 1 : 
				outFragColor.rgb *= vec3(0.25f, 1.0f, 0.25f);
				break;
			case 2 : 
				outFragColor.rgb *= vec3(0.25f, 0.25f, 1.0f);
				break;
			case 3 : 
				outFragColor.rgb *= vec3(1.0f, 1.0f, 0.25f);
				break;
		}
	}
}