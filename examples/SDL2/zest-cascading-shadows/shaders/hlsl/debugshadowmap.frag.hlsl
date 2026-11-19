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
Texture2DArray zest_texture_arrays[] : register(t0, space103);

struct varyings {
	float4 position : SV_Position;
	float2 uv : TEXCOORD0;
};

float4 main(varyings input) : SV_Target0 {
	float depth = zest_texture_arrays[NonUniformResourceIndex(shadow_index)].Sample(zest_samplers[NonUniformResourceIndex(shadow_sampler_index)], float3(input.uv, float(display_cascade_index))).r;
	return float4(depth, depth, depth, 1.0);
}
