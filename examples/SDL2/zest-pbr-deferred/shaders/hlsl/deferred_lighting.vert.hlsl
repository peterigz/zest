struct varyings {
	float4 position : SV_Position;
	float2 uv : TEXCOORD0;
};

//A full screen triangle built from the vertex index, with no vertex buffer.
//Vertex 0: (-1, -1), Vertex 1: (3, -1), Vertex 2: (-1, 3)
varyings main(uint vertex_index : SV_VertexID) {
	varyings output;
	output.uv = float2((vertex_index << 1) & 2, vertex_index & 2);
	output.position = float4(output.uv * 2.0 - 1.0, 0.0, 1.0);
	return output;
}
