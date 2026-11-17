struct instance_varyings {
	float4 position : SV_Position;
	float4 color : COLOR0;
};

//Two triangles covering the viewport, so the instance data only carries the color the verify shader checks
static const float2 positions[6] = {
	float2(-1.0, -1.0), float2( 1.0, -1.0), float2(-1.0,  1.0),
	float2( 1.0, -1.0), float2( 1.0,  1.0), float2(-1.0,  1.0)
};

instance_varyings main(float4 instance_color : POSITION0, uint vertex_id : SV_VertexID) {
	instance_varyings output;
	output.position = float4(positions[vertex_id], 0.0, 1.0);
	output.color = instance_color;
	return output;
}
