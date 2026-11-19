cbuffer zest_push : register(b0, space0) {
	uint sampler_index;
	uint target_index;
};

SamplerState zest_samplers[] : register(s0, space100);
Texture2D zest_textures[] : register(t0, space101);

struct varyings {
	float4 position : SV_Position;
	float2 uv : TEXCOORD0;
};

float4 main(varyings input) : SV_Target0 {
	return zest_textures[NonUniformResourceIndex(target_index)].Sample(zest_samplers[NonUniformResourceIndex(sampler_index)], input.uv);
}
