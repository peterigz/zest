cbuffer zest_push : register(b0, space0) {
	uint sampler_index;
	uint planet_index;
	uint rock_index;
	uint skybox_index;
	uint ubo_index;
};

struct sky_box_view_t {
	float4x4 proj;
	float4x4 view;
	float4 light_pos;
};

ConstantBuffer<sky_box_view_t> zest_uniforms[] : register(b0, space107);

struct varyings {
	float4 position : SV_Position;
	float3 uvw : TEXCOORD0;
};

varyings main(float3 vertex_position : POSITION0) {
	sky_box_view_t ubo = zest_uniforms[NonUniformResourceIndex(ubo_index)];
	varyings output;
	output.uvw = vertex_position;
	output.uvw.y *= -1;
	//The view's rotation only, so the sky box stays centred on the camera
	float3x3 view_rotation = (float3x3)ubo.view;
	float4x4 view_no_translation = float4x4(float4(view_rotation[0], 0.0), float4(view_rotation[1], 0.0), float4(view_rotation[2], 0.0), float4(0.0, 0.0, 0.0, 1.0));
	float4 clip_position = mul(mul(ubo.proj, view_no_translation), float4(vertex_position.xyz, 1.0));
	output.position = clip_position.xyww;
	return output;
}
