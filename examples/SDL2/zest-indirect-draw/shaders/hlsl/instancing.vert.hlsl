cbuffer zest_push : register(b0, space0) {
	uint sampler_index;
	uint planet_index;
	uint rock_index;
	uint skybox_index;
	uint ubo_index;
};

struct view_t {
	float4x4 proj;
	float4x4 view;
	float4 planes[6];
	float4 light_pos;
	float loc_speed;
	float glob_speed;
	uint visible;
};

ConstantBuffer<view_t> zest_uniforms[] : register(b0, space107);

struct varyings {
	float4 position : SV_Position;
	float3 normal : NORMAL0;
	float3 color : COLOR0;
	float3 uv : TEXCOORD0;
	float3 view_vec : TEXCOORD1;
	float3 light_vec : TEXCOORD2;
};

//GLSL constructors fill columns and HLSL constructors fill rows, so matrices built in the shader go through this
float3x3 columns3(float3 column0, float3 column1, float3 column2) {
	return transpose(float3x3(column0, column1, column2));
}

//Locations 0 to 3 are per vertex, and 4 to 7 per visible instance written by the cull pass
varyings main(float3 in_position : POSITION0, float4 in_color : COLOR0, float3 in_normal : NORMAL0, float2 in_uv : TEXCOORD0,
	float3 instance_position : INSTANCE_POSITION0, float3 instance_rotation : INSTANCE_ROTATION0, float3 instance_scale : INSTANCE_SCALE0, uint instance_texture_index : INSTANCE_TEXTURE_INDEX0) {
	view_t ubo = zest_uniforms[NonUniformResourceIndex(ubo_index)];
	varyings output;
	output.color = in_color.rgb;
	output.uv = float3(in_uv, float(instance_texture_index));

	//Rotate around x
	float s = sin(instance_rotation.x + ubo.loc_speed);
	float c = cos(instance_rotation.x + ubo.loc_speed);
	float3x3 mx = columns3(float3(c, s, 0.0), float3(-s, c, 0.0), float3(0.0, 0.0, 1.0));

	//Rotate around y
	s = sin(instance_rotation.y + ubo.loc_speed);
	c = cos(instance_rotation.y + ubo.loc_speed);
	float3x3 my = columns3(float3(c, 0.0, s), float3(0.0, 1.0, 0.0), float3(-s, 0.0, c));

	//Rotate around z
	s = sin(instance_rotation.z + ubo.loc_speed);
	c = cos(instance_rotation.z + ubo.loc_speed);
	float3x3 mz = columns3(float3(1.0, 0.0, 0.0), float3(0.0, c, s), float3(0.0, -s, c));

	float3x3 rotation = mul(mul(mz, my), mx);

	float4 local_position = float4(mul(in_position.xyz, rotation), 1.0);
	float4 position = float4((local_position.xyz * instance_scale) + instance_position, 1.0);

	output.position = mul(mul(ubo.proj, ubo.view), position);
	//The rotation is orthonormal, so its inverse is its transpose
	output.normal = mul(mul((float3x3)ubo.view, transpose(rotation)), in_normal);

	position = mul(ubo.view, float4(in_position.xyz + instance_position, 1.0));
	float3 light_position = mul((float3x3)ubo.view, ubo.light_pos.xyz);
	output.light_vec = light_position - position.xyz;
	output.view_vec = -position.xyz;
	return output;
}
