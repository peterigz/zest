cbuffer zest_push : register(b0, space0) {
	uint sampler_index;
	uint texture_index;
	uint uniform_index;
};

SamplerState zest_samplers[] : register(s0, space100);
Texture2D zest_textures[] : register(t0, space101);

struct varyings {
	float4 position : SV_Position;
	float4 uv : TEXCOORD0;
	float4 color : COLOR0;
};

//Additive: alpha is 0, and rgb is the tint faded along the ribbon (uv.w) by the texture's alpha
float4 main(varyings input) : SV_Target0 {
	float4 texel = zest_textures[NonUniformResourceIndex(texture_index)].Sample(zest_samplers[NonUniformResourceIndex(sampler_index)], input.uv.xy);
	return float4(input.color.rgb * input.uv.w * texel.a, 0.0);
}
