cbuffer zest_push : register(b0, space0) {
	uint sampler_index;
	uint planet_index;
	uint rock_index;
	uint skybox_index;
	uint ubo_index;
};

struct planet_view_t {
	float4x4 projection;
	float4x4 modelview;
	float4 planes[6];
	float4 light_pos;
	float loc_speed;
	float glob_speed;
	uint visible;
};

ConstantBuffer<planet_view_t> zest_uniforms[] : register(b0, space107);

struct varyings {
	float4 position : SV_Position;
	float3 normal : NORMAL0;
	float3 color : COLOR0;
	float2 uv : TEXCOORD0;
	float3 view_vec : TEXCOORD1;
	float3 light_vec : TEXCOORD2;
};

varyings main(float3 in_position : POSITION0, float4 in_color : COLOR0, float3 in_normal : NORMAL0, float2 in_uv : TEXCOORD0) {
	planet_view_t ubo = zest_uniforms[NonUniformResourceIndex(ubo_index)];
	varyings output;
	output.color = in_color.rgb;
	output.uv = in_uv;
	output.position = mul(mul(ubo.projection, ubo.modelview), float4(in_position.xyz, 1.0));

	float4 position = mul(ubo.modelview, float4(in_position, 1.0));
	output.normal = mul((float3x3)ubo.modelview, in_normal);
	float3 light_position = mul((float3x3)ubo.modelview, ubo.light_pos.xyz);
	output.light_vec = light_position - position.xyz;
	output.view_vec = -position.xyz;
	return output;
}
