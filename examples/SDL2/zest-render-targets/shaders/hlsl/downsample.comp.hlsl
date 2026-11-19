cbuffer zest_push : register(b0, space0) {
	uint storage_image_index;
	uint src_mip_index;
	uint downsampler_mip_index;
	uint sampler_index;
};

SamplerState zest_samplers[] : register(s0, space100);
Texture2D zest_textures[] : register(t0, space101);
RWTexture2D<float4> zest_images[] : register(u0, space106);

//Compute has no derivatives, so the source is read at its only mip
float3 sample_source(float2 uv) {
	return zest_textures[NonUniformResourceIndex(src_mip_index)].SampleLevel(zest_samplers[NonUniformResourceIndex(sampler_index)], uv, 0).rgb;
}

[numthreads(8, 8, 1)]
void main(uint3 thread_id : SV_DispatchThreadID) {
	int2 pixel_coords = int2(thread_id.xy);

	//Get the dimensions of the image we are writing to (the destination mip)
	uint image_width;
	uint image_height;
	zest_images[NonUniformResourceIndex(storage_image_index)].GetDimensions(image_width, image_height);
	int2 image_dims = int2(image_width, image_height);

	//Bounds check to terminate extra threads
	if (pixel_coords.x >= image_dims.x || pixel_coords.y >= image_dims.y) {
		return;
	}

	//The source mip is one level larger than the destination
	int2 src_dims = image_dims * 2;
	float2 src_texel_size = 1.0 / float2(src_dims);
	float x = src_texel_size.x;
	float y = src_texel_size.y;

	//Calculate the UV in the source texture that corresponds to the current pixel in the destination texture
	float2 uv = (float2(pixel_coords) + 0.5) / float2(image_dims);

	//Take 13 samples from the source texture around the calculated UV
	float3 a = sample_source(float2(uv.x - 2 * x, uv.y + 2 * y));
	float3 b = sample_source(float2(uv.x,         uv.y + 2 * y));
	float3 c = sample_source(float2(uv.x + 2 * x, uv.y + 2 * y));

	float3 d = sample_source(float2(uv.x - 2 * x, uv.y));
	float3 e = sample_source(float2(uv.x,         uv.y));
	float3 f = sample_source(float2(uv.x + 2 * x, uv.y));

	float3 g = sample_source(float2(uv.x - 2 * x, uv.y - 2 * y));
	float3 h = sample_source(float2(uv.x,         uv.y - 2 * y));
	float3 i = sample_source(float2(uv.x + 2 * x, uv.y - 2 * y));

	float3 j = sample_source(float2(uv.x - x, uv.y + y));
	float3 k = sample_source(float2(uv.x + x, uv.y + y));
	float3 l = sample_source(float2(uv.x - x, uv.y - y));
	float3 m = sample_source(float2(uv.x + x, uv.y - y));

	//The weights sum to 0.125 * 5 + 0.03125 * 4 + 0.0625 * 4 = 1, so the downsample preserves energy
	float3 downsample = e * 0.125;
	downsample += (a + c + g + i) * 0.03125;
	downsample += (b + d + f + h) * 0.0625;
	downsample += (j + k + l + m) * 0.125;

	//Write the final color to the destination mip level
	zest_images[NonUniformResourceIndex(storage_image_index)][pixel_coords] = float4(downsample, 1.0);
}
