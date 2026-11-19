cbuffer zest_push : register(b0, space0) {
	float4 rect;		//x, y, width, height in normalised device coordinates
	float4 tint;
	uint sampler_index;
	uint image_index;
};

struct varyings {
	float4 position : SV_Position;
	float2 uv : TEXCOORD0;
};

//No vertex buffer is bound. A 4 vertex triangle strip draw builds the quad from the vertex index.
varyings main(uint vertex_index : SV_VertexID) {
	float2 corner = float2(float(vertex_index & 1), float(vertex_index >> 1));
	varyings output;
	output.uv = corner;
	output.position = float4(rect.xy + corner * rect.zw, 0.0, 1.0);
	return output;
}
