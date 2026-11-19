cbuffer zest_push : register(b0, space0) {
	uint storage_image_index;		//upsample texture write to index
	uint src_mip_index;				//sample from mip index in the up sample texture
	uint downsampler_mip_index;		//downsampler texture mip index
	uint sampler_index;
};

SamplerState zest_samplers[] : register(s0, space100);
Texture2D zest_textures[] : register(t0, space101);
RWTexture2D<float4> zest_images[] : register(u0, space106);

//Compute has no derivatives, so each mip view is read at its only mip
float3 sample_mip(uint texture_index, float2 uv) {
	return zest_textures[NonUniformResourceIndex(texture_index)].SampleLevel(zest_samplers[NonUniformResourceIndex(sampler_index)], uv, 0).rgb;
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

	//The filter kernel is applied with a radius, specified in texture coordinates, so that the radius will vary across mip resolutions
	float x_offset_uv = 1.0 / float(image_dims.x);
	float y_offset_uv = 1.0 / float(image_dims.y);

	//Calculate the UV in the source texture that corresponds to the current pixel in the destination texture
	float2 uv = (float2(pixel_coords) + 0.5) / float2(image_dims);

	//Take 9 samples from the upsampler texture around current texel
	float3 a = sample_mip(src_mip_index, float2(uv.x - x_offset_uv, uv.y + y_offset_uv));
	float3 b = sample_mip(src_mip_index, float2(uv.x,               uv.y + y_offset_uv));
	float3 c = sample_mip(src_mip_index, float2(uv.x + x_offset_uv, uv.y + y_offset_uv));

	float3 d = sample_mip(src_mip_index, float2(uv.x - x_offset_uv, uv.y));
	float3 e = sample_mip(src_mip_index, float2(uv.x,               uv.y));
	float3 f = sample_mip(src_mip_index, float2(uv.x + x_offset_uv, uv.y));

	float3 g = sample_mip(src_mip_index, float2(uv.x - x_offset_uv, uv.y - y_offset_uv));
	float3 h = sample_mip(src_mip_index, float2(uv.x,               uv.y - y_offset_uv));
	float3 i = sample_mip(src_mip_index, float2(uv.x + x_offset_uv, uv.y - y_offset_uv));

	//Sample from the corresponding mip in the downsampler chain
	float3 downsample_color = sample_mip(downsampler_mip_index, uv);

	//Apply weighted distribution (3x3 tent filter) for a nice blur
	float3 upsample = e * 4.0;
	upsample += (b + d + f + h) * 2.0;
	upsample += (a + c + g + i);
	upsample *= 1.0 / 16.0;

	//Additively blend the blurred upsample result with the downsample result
	float4 final_color = float4(upsample + downsample_color, 1.0);

	//Write the final blended color to the storage image
	zest_images[NonUniformResourceIndex(storage_image_index)][pixel_coords] = final_color;
}
