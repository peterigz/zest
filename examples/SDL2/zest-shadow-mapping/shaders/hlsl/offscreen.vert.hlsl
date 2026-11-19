cbuffer zest_push : register(b0, space0) {
	uint sampler_index;
	uint shadow_index;
	uint view_uniform_index;
	uint offscreen_uniform_index;
	bool enable_pcf;
};

struct offscreen_view_t {
	float4x4 depth_mvp;
};

ConstantBuffer<offscreen_view_t> zest_uniforms[] : register(b0, space107);

//Every input the pipeline supplies is declared, as attribute location N is the Nth input, read or not
float4 main(float3 in_position : POSITION0, float3 instance_position : INSTANCE_POSITION0, float4 instance_color : INSTANCE_COLOR0, float3 instance_rotation : INSTANCE_ROTATION0, float3 instance_scale : INSTANCE_SCALE0) : SV_Position {
	return mul(zest_uniforms[NonUniformResourceIndex(offscreen_uniform_index)].depth_mvp, float4(in_position, 1.0));
}
