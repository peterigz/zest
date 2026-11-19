cbuffer zest_push : register(b0, space0) {
	uint texture_index;
	uint sampler_index;
	uint uniform_index;
};

SamplerState zest_samplers[] : register(s0, space100);
Texture2DArray zest_texture_arrays[] : register(t0, space103);

struct varyings {
	float4 position : SV_Position;
	float4 frag_color : COLOR0;
	float3 tex_coord : TEXCOORD0;
};

//2d/3d billboard fragment shader
float4 main(varyings input) : SV_Target0 {
	float4 texel = zest_texture_arrays[NonUniformResourceIndex(texture_index)].Sample(zest_samplers[NonUniformResourceIndex(sampler_index)], input.tex_coord);
	float4 out_color;
	//Pre multiply alpha
	out_color.rgb = texel.rgb * input.frag_color.rgb * texel.a;
	//If frag_color.a is 0 then color will be additive. The higher the value of a the more alpha blended the color will be.
	out_color.a = texel.a * input.frag_color.a;
	return out_color;
}
