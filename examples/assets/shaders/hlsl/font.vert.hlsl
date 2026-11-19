cbuffer zest_push : register(b0, space0) {
	float4 transform;
	float4 shadow_color;
	float2 shadow_offset;		//In screen pixels
	float2 unit_range;
	float in_bias;
	float out_bias;
	float smoothness;
	float gamma;
	uint sampler_index;
	uint image_index;
	uint binding_number;		//1 for Texture2D, 3 for Texture2DArray
};

//Quad indexes
static const int indexes[6] = { 0, 1, 2, 2, 1, 3 };
static const float scale_max_value = 4096.0 / 32767.0;

struct varyings {
	float4 position : SV_Position;
	float3 tex_coord : TEXCOORD0;
	float4 color : COLOR0;
};

//One instance per glyph, drawn as 6 vertices
varyings main(float2 in_position : POSITION0, float4 uv : TEXCOORD0, float4 color : COLOR0, int2 scale : SCALE0, uint texture_array_index : TEXCOORD1, uint vertex_index : SV_VertexID) {
	float2 size = float2(scale) * scale_max_value;

	float2 uvs[4];
	uvs[0] = float2(uv.x, uv.y);
	uvs[1] = float2(uv.z, uv.y);
	uvs[2] = float2(uv.x, uv.w);
	uvs[3] = float2(uv.z, uv.w);

	float2 bounds[4];
	bounds[0] = float2(0.0, 0.0);
	bounds[3] = size;
	bounds[1] = float2(bounds[3].x, bounds[0].y);
	bounds[2] = float2(bounds[0].x, bounds[3].y);

	int index = indexes[vertex_index];

	float2 vertex_position = bounds[index] + in_position;
	varyings output;
	output.position = float4(vertex_position * transform.xy + transform.zw, 0.0, 1.0);
	output.tex_coord = float3(uvs[index], float(texture_array_index));
	output.color = color;
	return output;
}
