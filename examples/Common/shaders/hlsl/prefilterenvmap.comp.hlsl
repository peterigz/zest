cbuffer zest_push : register(b0, space0) {
	uint source_index;
	uint dest_index;
	uint sampler_index;
	float mip_roughness;
	uint sample_count;
};

//Pre-filters an environment cubemap for specular IBL, one roughness per mip, one mip per dispatch
SamplerState zest_samplers[] : register(s0, space100);
TextureCube zest_cubemaps[] : register(t0, space102);
//A cube storage image is written as its array of six faces, and dest_index is one mip's view
RWTexture2DArray<float4> zest_image_arrays[] : register(u0, space206);

static const float PI = 3.14159265359;

//Based on http://byteblacksmith.com/improvements-to-the-canonical-one-liner-glsl-rand-for-opengl-es-2-0/
float random(float2 co) {
	float a = 12.9898;
	float b = 78.233;
	float c = 43758.5453;
	float dot_product = dot(co.xy, float2(a, b));
	//GLSL mod, which floors where fmod truncates
	float wrapped = dot_product - PI * floor(dot_product / PI);
	return frac(sin(wrapped) * c);
}

//Radical inverse based on http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
float2 hammersley_2d(uint sample_index, uint total_samples) {
	uint bits = (sample_index << 16u) | (sample_index >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
	float radical_inverse = float(bits) * 2.3283064365386963e-10;
	return float2(float(sample_index) / float(total_samples), radical_inverse);
}

//Based on http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_slides.pdf
float3 importance_sample_ggx(float2 sample_point, float roughness, float3 normal) {
	//Maps a 2D point to a hemisphere with spread based on roughness
	float alpha = roughness * roughness;
	float phi = 2.0 * PI * sample_point.x + random(normal.xz) * 0.1;
	float cos_theta = sqrt((1.0 - sample_point.y) / (1.0 + (alpha * alpha - 1.0) * sample_point.y));
	float sin_theta = sqrt(1.0 - cos_theta * cos_theta);
	float3 half_vector = float3(sin_theta * cos(phi), sin_theta * sin(phi), cos_theta);

	//Tangent space
	float3 up = abs(normal.z) < 0.999 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
	float3 tangent_x = normalize(cross(up, normal));
	float3 tangent_y = normalize(cross(normal, tangent_x));

	//Convert to world space
	return normalize(tangent_x * half_vector.x + tangent_y * half_vector.y + normal * half_vector.z);
}

//Normal distribution function
float d_ggx(float dot_nh, float roughness) {
	float alpha = roughness * roughness;
	float alpha2 = alpha * alpha;
	float denom = dot_nh * dot_nh * (alpha2 - 1.0) + 1.0;
	return (alpha2) / (PI * denom * denom);
}

float3 prefilter_env_map(float3 R, float roughness) {
	float3 N = R;
	float3 V = R;
	float3 color = float3(0.0, 0.0, 0.0);
	float total_weight = 0.0;
	uint env_width;
	uint env_height;
	uint env_mip_count;
	zest_cubemaps[NonUniformResourceIndex(source_index)].GetDimensions(0, env_width, env_height, env_mip_count);
	float env_map_dim = float(env_width);
	for (uint sample_index = 0u; sample_index < sample_count; sample_index++) {
		float2 sample_point = hammersley_2d(sample_index, sample_count);
		float3 H = importance_sample_ggx(sample_point, roughness, N);
		float3 L = 2.0 * dot(V, H) * H - V;
		float dot_nl = clamp(dot(N, L), 0.0, 1.0);
		if (dot_nl > 0.0) {
			//Filtering based on https://placeholderart.wordpress.com/2015/07/28/implementation-notes-runtime-environment-map-filtering-for-image-based-lighting/
			float dot_nh = clamp(dot(N, H), 0.0, 1.0);
			float dot_vh = clamp(dot(V, H), 0.0, 1.0);

			//Probability distribution function
			float pdf = d_ggx(dot_nh, roughness) * dot_nh / (4.0 * dot_vh) + 0.0001;
			//Solid angle of current sample
			float omega_s = 1.0 / (float(sample_count) * pdf);
			//Solid angle of 1 pixel across all cube faces
			float omega_p = 4.0 * PI / (6.0 * env_map_dim * env_map_dim);
			//Biased (+1.0) mip level for better result
			float mip_level = roughness == 0.0 ? 0.0 : max(0.5 * log2(omega_s / omega_p) + 1.0, 0.0f);
			color += zest_cubemaps[NonUniformResourceIndex(source_index)].SampleLevel(zest_samplers[NonUniformResourceIndex(sampler_index)], L, mip_level).rgb * dot_nl;
			total_weight += dot_nl;
		}
	}
	return (total_weight > 0.0) ? (color / total_weight) : float3(0.0, 0.0, 0.0);
}

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
	zest_image_arrays[NonUniformResourceIndex(dest_index)].GetDimensions(width, height, face_count);

	//Boundary check
	if (pixel.x >= width || pixel.y >= height) {
		return;
	}

	//Get the reflection vector for the current pixel on the cubemap
	float3 R = get_direction_from_uv(pixel.xy, pixel.z, float2(width, height));

	float4 out_color = float4(prefilter_env_map(R, mip_roughness), 1.0);

	//Store the final pre-filtered color in the destination cubemap
	zest_image_arrays[NonUniformResourceIndex(dest_index)][pixel] = out_color;
}
