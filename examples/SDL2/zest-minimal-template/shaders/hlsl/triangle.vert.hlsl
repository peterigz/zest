struct varyings {
	float4 position : SV_Position;
	float3 color : COLOR0;
};

//No vertex buffer is bound. The position and color are looked up from constant
//arrays using the vertex index, so a 3 vertex draw is all that's needed.
varyings main(uint vertex_index : SV_VertexID) {
	const float2 positions[3] = { float2(0.0, -0.6), float2(0.6, 0.6), float2(-0.6, 0.6) };
	const float3 colors[3] = { float3(1.0, 0.0, 0.0), float3(0.0, 1.0, 0.0), float3(0.0, 0.0, 1.0) };

	varyings output;
	output.position = float4(positions[vertex_index], 0.0, 1.0);
	output.color = colors[vertex_index];
	return output;
}
