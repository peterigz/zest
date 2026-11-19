cbuffer zest_push : register(b0, space0) {
	uint sampler_index;
	uint planet_index;
	uint rock_index;
	uint skybox_index;
	uint ubo_index;
};

SamplerState zest_samplers[] : register(s0, space100);
TextureCube zest_cubemaps[] : register(t0, space102);

struct varyings {
	float4 position : SV_Position;
	float3 uvw : TEXCOORD0;
};

float4 main(varyings input) : SV_Target0 {
	float3 color = zest_cubemaps[NonUniformResourceIndex(skybox_index)].SampleLevel(zest_samplers[NonUniformResourceIndex(sampler_index)], input.uvw, 0.0).rgb;
	return float4(color, 1.0);
}
