cbuffer zest_push : register(b0, space0) {
	uint sampler_index;
	uint shadow_sampler_index;
	uint shadow_index;
	uint vert_index;
	uint frag_index;
	uint cascade_index;
	uint texture_index;
	uint display_cascade_index;
	int enable_pcf;
};

#define SHADOW_MAP_CASCADE_COUNT 4

struct cascade_view_t {
	float4x4 cascade_view_proj[SHADOW_MAP_CASCADE_COUNT];
};

ConstantBuffer<cascade_view_t> zest_uniforms[] : register(b0, space107);

struct varyings {
	float4 position : SV_Position;
	float3 uv : TEXCOORD0;
};

//GLSL constructors fill columns and HLSL constructors fill rows, so matrices built in the shader go through this
float3x3 columns3(float3 column0, float3 column1, float3 column2) {
	return transpose(float3x3(column0, column1, column2));
}

//One view per cascade, so the view index picks the cascade's matrix
varyings main(float3 vertex_position : POSITION0, float2 uv : TEXCOORD0, float3 instance_position : INSTANCE_POSITION0, float3 instance_rotation : INSTANCE_ROTATION0, float3 instance_scale : INSTANCE_SCALE0, uint view_index : SV_ViewID) {
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

	varyings output;
	output.position = mul(zest_uniforms[NonUniformResourceIndex(cascade_index)].cascade_view_proj[view_index], float4(position, 1.0));
	output.uv = float3(uv, 0);
	return output;
}
