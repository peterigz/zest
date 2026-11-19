cbuffer zest_push : register(b0, space0) {
	uint view_index;
	uint lights_index;
};

struct view_t {
	float4x4 view;
	float4x4 proj;
	float2 screen_size;
	float timer_lerp;
	float update_time;
};

ConstantBuffer<view_t> zest_uniforms[] : register(b0, space107);

struct varyings {
	float4 position : SV_Position;
	float3 uvw : TEXCOORD0;
};

//The sky box pipeline is a copy of the PBR pipeline, so every one of its inputs is declared, as attribute location N is the Nth input
varyings main(float3 vertex_position : POSITION0, float4 vertex_color : COLOR0, float3 vertex_normal : NORMAL0, float2 in_uv : TEXCOORD0, float4 tangent : TANGENT0, uint group_id : GROUP_ID0,
	float3 instance_position : INSTANCE_POSITION0, float4 instance_color : INSTANCE_COLOR0, float3 instance_rotation : INSTANCE_ROTATION0, float3 instance_scale : INSTANCE_SCALE0) {
	view_t ubo = zest_uniforms[NonUniformResourceIndex(view_index)];
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
