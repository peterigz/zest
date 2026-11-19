cbuffer zest_push : register(b0, space0) {
	uint sampler_index;
	uint texture_index;
	uint uniform_index;
};

struct view_t {
	float4x4 view;
	float4x4 proj;
};

ConstantBuffer<view_t> zest_uniforms[] : register(b0, space107);

struct varyings {
	float4 position : SV_Position;
	float4 uv : TEXCOORD0;
	float4 color : COLOR0;
};

varyings main(float4 vertex_position : POSITION0, float4 uv : TEXCOORD0, float4 color : COLOR0) {
	view_t ubo = zest_uniforms[NonUniformResourceIndex(uniform_index)];
	varyings output;
	output.position = mul(mul(ubo.proj, ubo.view), float4(vertex_position.xyz, 1.0));
	output.uv = uv;
	output.color = color;
	return output;
}
