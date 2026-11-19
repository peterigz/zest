cbuffer zest_push : register(b0, space0) {
	float4 rect;
	float4 tint;
	uint sampler_index;
	uint image_index;
};

SamplerState zest_samplers[] : register(s0, space100);
Texture2D zest_textures[] : register(t0, space101);

struct varyings {
	float4 position : SV_Position;
	float2 uv : TEXCOORD0;
};

//Both quads sample an r8_unorm image. The shader is identical for each - the only difference is
//the component mapping baked into the image view that image_index points at.
float4 main(varyings input) : SV_Target0 {
	float4 texel = zest_textures[NonUniformResourceIndex(image_index)].Sample(zest_samplers[NonUniformResourceIndex(sampler_index)], input.uv);
	//The pipeline blends premultiplied alpha, so scale rgb by the final alpha
	float alpha = texel.a * tint.a;
	return float4(texel.rgb * tint.rgb * alpha, alpha);
}
