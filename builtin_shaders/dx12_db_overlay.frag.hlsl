//The debug overlay's pixel shader on D3D12, which samples one slice of the font array per character
cbuffer zest_push : register(b0, space0) {
	float4 transform;
	uint font_index;
	uint sampler_index;
};
SamplerState zest_samplers[] : register(s0, space100);
Texture2DArray zest_texture_arrays[] : register(t0, space103);

struct zest_overlay_varyings {
	float4 position : SV_Position;
	float4 color : COLOR0;
	float2 uv : TEXCOORD0;
	nointerpolation float layer : TEXCOORD1;
};

float4 main(zest_overlay_varyings input) : SV_Target0 {
	float alpha = zest_texture_arrays[NonUniformResourceIndex(font_index)].Sample(zest_samplers[NonUniformResourceIndex(sampler_index)], float3(input.uv, input.layer)).r;
	return float4(input.color.rgb, input.color.a * alpha);
}
