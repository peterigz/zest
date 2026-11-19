cbuffer zest_push : register(b0, space0) {
	uint sampler_index;
	uint shadow_index;
	uint uniform_index;
	uint offscreen_uniform_index;
	bool enable_pcf;
};

struct scene_view_t {
	float4x4 projection;
	float4x4 view;
	float4x4 model;
	float4x4 light_space;
	float4 light_pos;
	float z_near;
	float z_far;
};

ConstantBuffer<scene_view_t> zest_uniforms[] : register(b0, space107);

struct varyings {
	float4 position : SV_Position;
	float3 normal : NORMAL0;
	float3 color : COLOR0;
	float3 view_vec : TEXCOORD0;
	float3 light_vec : TEXCOORD1;
	float4 shadow_coord : TEXCOORD2;
};

//GLSL constructors fill columns and HLSL constructors fill rows, so matrices built in the shader go through this
float4x4 columns4(float4 column0, float4 column1, float4 column2, float4 column3) {
	return transpose(float4x4(column0, column1, column2, column3));
}

//Every input the pipeline supplies is declared, as attribute location N is the Nth input, read or not
varyings main(float3 vertex_position : POSITION0, float4 vertex_color : COLOR0, float3 vertex_normal : NORMAL0, float2 in_uv : TEXCOORD0,
	float3 instance_position : INSTANCE_POSITION0, float4 instance_color : INSTANCE_COLOR0, float3 instance_rotation : INSTANCE_ROTATION0, float3 instance_scale : INSTANCE_SCALE0) {
	scene_view_t ubo = zest_uniforms[NonUniformResourceIndex(uniform_index)];
	//Maps clip space x and y from -1..1 to texture coordinates 0..1
	float4x4 bias_mat = columns4(float4(0.5, 0.0, 0.0, 0.0), float4(0.0, 0.5, 0.0, 0.0), float4(0.0, 0.0, 1.0, 0.0), float4(0.5, 0.5, 0.0, 1.0));

	varyings output;
	output.color = vertex_color.rgb * instance_color.rgb;
	output.normal = vertex_normal;

	output.position = mul(mul(mul(ubo.projection, ubo.view), ubo.model), float4(vertex_position.xyz, 1.0));

	float4 position = mul(ubo.model, float4(vertex_position, 1.0));
	output.normal = mul((float3x3)ubo.model, vertex_normal);
	output.light_vec = normalize(ubo.light_pos.xyz - vertex_position);
	output.view_vec = -position.xyz;

	output.shadow_coord = mul(mul(mul(bias_mat, ubo.light_space), ubo.model), float4(vertex_position, 1.0));
	return output;
}
