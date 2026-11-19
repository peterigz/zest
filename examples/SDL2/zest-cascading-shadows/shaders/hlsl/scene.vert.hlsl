cbuffer zest_push : register(b0, space0) {
	uint sampler_index;
	uint shadow_sampler_index;
	uint shadow_index;
	uint vert_index;
	uint frag_index;
	uint cascade_index;
	uint texture_index;
	uint display_cascade_index;
	bool enable_pcf;
};

struct scene_view_t {
	float4x4 projection;
	float4x4 view;
	float4x4 model;
};

ConstantBuffer<scene_view_t> zest_uniforms[] : register(b0, space107);

struct varyings {
	float4 position : SV_Position;
	float3 normal : NORMAL0;
	float3 color : COLOR0;
	float3 view_position : TEXCOORD0;
	float3 world_position : TEXCOORD1;
	float3 uv : TEXCOORD2;
};

//GLSL constructors fill columns and HLSL constructors fill rows, so matrices built in the shader go through this
float3x3 columns3(float3 column0, float3 column1, float3 column2) {
	return transpose(float3x3(column0, column1, column2));
}

//Every input the pipeline supplies is declared, as attribute location N is the Nth input, read or not
varyings main(float3 vertex_position : POSITION0, float4 vertex_color : COLOR0, float3 vertex_normal : NORMAL0, float2 in_uv : TEXCOORD0,
	float3 instance_position : INSTANCE_POSITION0, float4 instance_color : INSTANCE_COLOR0, float3 instance_rotation : INSTANCE_ROTATION0, float3 instance_scale : INSTANCE_SCALE0) {
	scene_view_t ubo = zest_uniforms[NonUniformResourceIndex(vert_index)];
	varyings output;
	output.color = vertex_color.rgb * instance_color.rgb;

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

	output.color = vertex_color.rgb;
	output.normal = mul((float3x3)ubo.model, mul(rotation_matrix, vertex_normal));
	output.uv = float3(in_uv, 0);
	output.world_position = position;
	output.view_position = mul(ubo.view, float4(position.xyz, 1.0)).xyz;
	output.position = mul(mul(mul(ubo.projection, ubo.view), ubo.model), float4(position.xyz, 1.0));
	return output;
}
