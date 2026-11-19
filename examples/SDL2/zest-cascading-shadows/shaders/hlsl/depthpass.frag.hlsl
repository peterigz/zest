cbuffer zest_push : register(b0, space0) {
	uint sampler_index;
	uint shadow_sampler_index;
	uint shadow_index;
	uint vert_index;
	uint frag_index;
	uint cascade_index;
	uint texture_index;
	uint display_cascade_index;
	int enable_pcf;
};

SamplerState zest_samplers[] : register(s0, space100);
Texture2D zest_textures[] : register(t0, space101);

struct varyings {
	float4 position : SV_Position;
	float3 uv : TEXCOORD0;
};

//Depth only, so the shader just alpha tests the foliage
void main(varyings input) {
	float alpha = zest_textures[NonUniformResourceIndex(texture_index)].Sample(zest_samplers[NonUniformResourceIndex(sampler_index)], input.uv.xy).a;
	if (alpha < 0.5) {
		discard;
	}
}
