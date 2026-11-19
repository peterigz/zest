cbuffer zest_push : register(b0, space0) {
	float3 material_color;		//color in the GLSL
	uint padding;
	uint view_index;
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
	float3 normal : NORMAL0;
	float4 color : COLOR0;
	float2 pbr : TEXCOORD0;
};

//GLSL constructors fill columns and HLSL constructors fill rows, so matrices built in the shader go through this
float3x3 columns3(float3 column0, float3 column1, float3 column2) {
	return transpose(float3x3(column0, column1, column2));
}

//Locations 0 to 5 are per vertex and 6 to 11 per instance
varyings main(float3 vertex_position : POSITION0, float4 vertex_color : COLOR0, float3 vertex_normal : NORMAL0, float2 in_uv : TEXCOORD0, float4 tangent : TANGENT0, uint group_id : GROUP_ID0,
	float3 instance_position : INSTANCE_POSITION0, float4 instance_color : INSTANCE_COLOR0, float3 instance_rotation : INSTANCE_ROTATION0, float roughness : ROUGHNESS0, float3 instance_scale : INSTANCE_SCALE0, float metallic : METALLIC0) {
	//Rotate around x
	float s = sin(instance_rotation.x);
	float c = cos(instance_rotation.x);
	float3x3 mx = columns3(float3(c, s, 0.0), float3(-s, c, 0.0), float3(0.0, 0.0, 1.0));

	//Rotate around y
	s = sin(instance_rotation.y);
	c = cos(instance_rotation.y);
	float3x3 my = columns3(float3(c, 0.0, s), float3(0.0, 1.0, 0.0), float3(-s, 0.0, c));

	//Rotate around z
	s = sin(instance_rotation.z);
	c = cos(instance_rotation.z);
	float3x3 mz = columns3(float3(1.0, 0.0, 0.0), float3(0.0, c, s), float3(0.0, -s, c));

	float3x3 rotation_matrix = mul(mul(mz, my), mx);
	float3 position = mul(vertex_position * instance_scale, rotation_matrix) + instance_position;

	view_t ubo = zest_uniforms[NonUniformResourceIndex(view_index)];
	varyings output;
	output.position = mul(mul(ubo.proj, ubo.view), float4(position, 1.0));

	float4 color = vertex_color * instance_color;

	output.normal = mul(vertex_normal, rotation_matrix);
	output.color = float4(material_color * color.rgb, 1.0);
	output.pbr = float2(roughness, metallic);
	return output;
}
