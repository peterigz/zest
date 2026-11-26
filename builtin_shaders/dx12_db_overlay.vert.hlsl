//The debug overlay's vertex shader on D3D12
cbuffer zest_push : register(b0, space0) {
	float4 transform;
	uint font_index;
	uint sampler_index;
};

struct zest_overlay_input {
	float2 position : POSITION0;
	float2 uv : TEXCOORD0;
	float4 color : COLOR0;
	uint character_index : TEXCOORD1;
};

struct zest_overlay_varyings {
	float4 position : SV_Position;
	float4 color : COLOR0;
	float2 uv : TEXCOORD0;
	nointerpolation float layer : TEXCOORD1;
};

zest_overlay_varyings main(zest_overlay_input input) {
	zest_overlay_varyings output;
	output.position = float4(input.position * transform.xy + transform.zw, 0.0, 1.0);
	output.color = input.color;
	output.uv = input.uv;
	output.layer = (float)input.character_index;
	return output;
}
