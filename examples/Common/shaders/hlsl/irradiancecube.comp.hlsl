cbuffer zest_push : register(b0, space0) {
	uint source_env_index;
	uint dest_irradiance_index;
	uint sampler_index;
	float delta_phi;
	float delta_theta;
};

//Convolves an environment cubemap into an irradiance map
SamplerState zest_samplers[] : register(s0, space100);
TextureCube zest_cubemaps[] : register(t0, space102);
//A cube storage image is written as its array of six faces
RWTexture2DArray<float4> zest_image_arrays[] : register(u0, space206);

static const float PI = 3.14159265358979323846;

//Converts a pixel coordinate (on a cube face) into a 3D direction vector
float3 get_direction_from_uv(uint2 pixel_coords, uint face_index, float2 size) {
	//Map pixel coords to the [-1, 1] normalized device coordinate range
	float2 uv = (float2(pixel_coords) + 0.5) / size;
	float2 ndc = uv * 2.0 - 1.0;

	switch (face_index) {
		case 0: return normalize(float3( 1.0, -ndc.y, -ndc.x));		//+X
		case 1: return normalize(float3(-1.0, -ndc.y,  ndc.x));		//-X
		case 2: return normalize(float3( ndc.x,  1.0,  ndc.y));		//+Y
		case 3: return normalize(float3( ndc.x, -1.0, -ndc.y));		//-Y
		case 4: return normalize(float3( ndc.x, -ndc.y,  1.0));		//+Z
		case 5: return normalize(float3(-ndc.x, -ndc.y, -1.0));		//-Z
	}
	return float3(0.0, 0.0, 0.0);
}

[numthreads(8, 8, 1)]
void main(uint3 thread_id : SV_DispatchThreadID) {
	uint3 pixel = thread_id;
	uint width;
	uint height;
	uint face_count;
	zest_image_arrays[NonUniformResourceIndex(dest_irradiance_index)].GetDimensions(width, height, face_count);

	//Boundary check
	if (pixel.x >= width || pixel.y >= height) {
		return;
	}

	//Get the normal for the current pixel on the cubemap
	float3 N = get_direction_from_uv(pixel.xy, pixel.z, float2(width, height));

	//Create a coordinate system around the normal
	float3 up = float3(0.0, 1.0, 0.0);
	float3 right = normalize(cross(up, N));
	up = cross(N, right);

	const float TWO_PI = PI * 2.0;
	const float HALF_PI = PI * 0.5;

	float3 color = float3(0.0, 0.0, 0.0);
	uint sample_count = 0u;
	//Convolve the environment map by sampling over the hemisphere around N
	for (float phi = 0.0; phi < TWO_PI; phi += delta_phi) {
		for (float theta = 0.0; theta < HALF_PI; theta += delta_theta) {
			float3 temp_vec = cos(phi) * right + sin(phi) * up;
			float3 sample_vector = cos(theta) * N + sin(theta) * temp_vec;
			//Accumulate color, weighted by cos(theta) for Lambertian diffusion
			color += zest_cubemaps[NonUniformResourceIndex(source_env_index)].SampleLevel(zest_samplers[NonUniformResourceIndex(sampler_index)], sample_vector, 0.0).rgb * cos(theta) * sin(theta);
			sample_count++;
		}
	}

	float4 out_color = float4(PI * color / float(sample_count), 1.0);
	//Store the final convoluted color in the destination irradiance map
	zest_image_arrays[NonUniformResourceIndex(dest_irradiance_index)][pixel] = out_color;
}
