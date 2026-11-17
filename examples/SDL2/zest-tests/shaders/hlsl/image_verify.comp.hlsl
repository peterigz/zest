cbuffer zest_push : register(b0, space0) {
	uint read_image_index;
	uint verify_buffer_index;
	uint sampler_index;
	uint width;
	uint height;
};

SamplerState zest_samplers[] : register(s0, space100);
Texture2D zest_textures[] : register(t0, space101);
RWByteAddressBuffer zest_buffers[] : register(u0, space105);

[numthreads(8, 8, 1)]
void main(uint3 thread_id : SV_DispatchThreadID) {
	if (thread_id.x >= width || thread_id.y >= height) {
		return;
	}
	float2 offset_uv = float2(1.0 / float(width), 1.0 / float(height));
	float3 color = zest_textures[NonUniformResourceIndex(read_image_index)].SampleLevel(zest_samplers[NonUniformResourceIndex(sampler_index)], offset_uv, 0).rgb;
	//The result is a float4: x is set on a match and y on a mismatch
	if (color.r == 0 && color.g == 1 && color.b == 1) {
		zest_buffers[NonUniformResourceIndex(verify_buffer_index)].Store(0, asuint(1.0));
	} else {
		zest_buffers[NonUniformResourceIndex(verify_buffer_index)].Store(4, asuint(1.0));
	}
}
