cbuffer zest_push : register(b0, space0) {
	uint image_index;
};

RWTexture2D<float4> zest_images[] : register(u0, space106);

static const uint NUM_SAMPLES = 1024;
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
float2 hammersley_2d(uint sample_index, uint sample_count) {
	uint bits = (sample_index << 16u) | (sample_index >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
	float radical_inverse = float(bits) * 2.3283064365386963e-10;
	return float2(float(sample_index) / float(sample_count), radical_inverse);
}

//Based on http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_slides.pdf
float3 importance_sample_ggx(float2 sample_point, float roughness, float3 normal) {
	float alpha = roughness * roughness;
	float phi = 2.0 * PI * sample_point.x + random(normal.xz) * 0.1;
	float cos_theta = sqrt((1.0 - sample_point.y) / (1.0 + (alpha * alpha - 1.0) * sample_point.y));
	float sin_theta = sqrt(1.0 - cos_theta * cos_theta);
	float3 half_vector = float3(sin_theta * cos(phi), sin_theta * sin(phi), cos_theta);

	float3 up = abs(normal.z) < 0.999 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
	float3 tangent_x = normalize(cross(up, normal));
	float3 tangent_y = normalize(cross(normal, tangent_x));

	return normalize(tangent_x * half_vector.x + tangent_y * half_vector.y + normal * half_vector.z);
}

float g_schlicksmith_ggx(float dot_nl, float dot_nv, float roughness) {
	float k = (roughness * roughness) / 2.0;
	float geometry_light = dot_nl / (dot_nl * (1.0 - k) + k);
	float geometry_view = dot_nv / (dot_nv * (1.0 - k) + k);
	return geometry_light * geometry_view;
}

float2 calculate_brdf(float dot_nv_input, float roughness) {
	//The normal always points along z for the 2D lookup
	const float3 normal = float3(0.0, 0.0, 1.0);
	float3 view = float3(sqrt(1.0 - dot_nv_input * dot_nv_input), 0.0, dot_nv_input);

	float2 lut = float2(0.0, 0.0);
	for (uint sample_index = 0u; sample_index != NUM_SAMPLES; ++sample_index) {
		float2 sample_point = hammersley_2d(sample_index, NUM_SAMPLES);
		float3 half_vector = importance_sample_ggx(sample_point, roughness, normal);
		float3 light = 2.0 * dot(view, half_vector) * half_vector - view;

		float dot_nl = max(dot(normal, light), 0.0);
		float dot_nv = max(dot(normal, view), 0.0);
		float dot_vh = max(dot(view, half_vector), 0.0);
		float dot_nh = max(dot(half_vector, normal), 0.0);

		if (dot_nl > 0.0) {
			float geometry = g_schlicksmith_ggx(dot_nl, dot_nv, roughness);
			float geometry_visibility = (geometry * dot_vh) / (dot_nh * dot_nv);
			float fresnel = pow(1.0 - dot_vh, 5.0);
			lut += float2((1.0 - fresnel) * geometry_visibility, fresnel * geometry_visibility);
		}
	}
	return lut / float(NUM_SAMPLES);
}

[numthreads(8, 8, 1)]
void main(uint3 thread_id : SV_DispatchThreadID) {
	uint width;
	uint height;
	zest_images[NonUniformResourceIndex(image_index)].GetDimensions(width, height);
	if (thread_id.x >= width || thread_id.y >= height) {
		return;
	}
	//Roughness is y and N.V is x
	float2 uv = (float2(thread_id.xy) + 0.5) / float2(width, height);
	float2 brdf = calculate_brdf(uv.x, uv.y);
	zest_images[NonUniformResourceIndex(image_index)][thread_id.xy] = float4(brdf, 0.0, 1.0);
}
