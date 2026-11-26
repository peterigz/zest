//D3D12's blit and mip generation: a full screen triangle that samples one slice of the source, with the point sampler when filter is 0
cbuffer zest_blit_constants : register(b0) {
	float4 source_rect;
	uint filter;
	uint3 padding;
};
Texture2DArray source : register(t0);
SamplerState linear_sampler : register(s0);
SamplerState point_sampler : register(s1);

struct zest_blit_varyings {
	float4 position : SV_Position;
	float2 uv : TEXCOORD0;
};

zest_blit_varyings vertex_main(uint vertex_id : SV_VertexID) {
	zest_blit_varyings output;
	float2 corner = float2((vertex_id << 1) & 2, vertex_id & 2);
	output.position = float4(corner * 2.0 - 1.0, 0.0, 1.0);
	output.uv = corner;
	return output;
}

float4 pixel_main(zest_blit_varyings input) : SV_Target {
	float3 location = float3(source_rect.xy + input.uv * source_rect.zw, 0.0);
	if (filter == 0) {
		return source.SampleLevel(point_sampler, location, 0.0);
	}
	return source.SampleLevel(linear_sampler, location, 0.0);
}
