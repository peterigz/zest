cbuffer zest_push : register(b0, space0) {
	uint base_index;
	uint bloom_index;
	uint sampler_index;
	float bloom_alpha;
};

SamplerState zest_samplers[] : register(s0, space100);
Texture2D zest_textures[] : register(t0, space101);

struct varyings {
	float4 position : SV_Position;
	float2 uv : TEXCOORD0;
};

float4 main(varyings input) : SV_Target0 {
	SamplerState linear_sampler = zest_samplers[NonUniformResourceIndex(sampler_index)];
	float4 base_color = zest_textures[NonUniformResourceIndex(base_index)].Sample(linear_sampler, input.uv);
	float4 bloom_color = zest_textures[NonUniformResourceIndex(bloom_index)].Sample(linear_sampler, input.uv) * bloom_alpha;
	return base_color + bloom_color;
}
