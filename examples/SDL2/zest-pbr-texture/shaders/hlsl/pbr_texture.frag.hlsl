cbuffer zest_push : register(b0, space0) {
	float4 camera;
	uint irradiance_index;
	uint brd_lookup_index;
	uint pre_filtered_index;
	uint sampler_index;
	uint view_buffer_index;
	uint lights_buffer_index;
	uint albedo_index;
	uint normal_index;
	uint ao_index;
	uint metallic_index;
	uint roughness_index;
};

struct lights_t {
	float4 lights[4];
	float exposure;
	float gamma;
};

SamplerState zest_samplers[] : register(s0, space100);
Texture2D zest_textures[] : register(t0, space101);
TextureCube zest_cubemaps[] : register(t0, space102);
ConstantBuffer<lights_t> zest_uniforms[] : register(b0, space107);

struct varyings {
	float4 position : SV_Position;
	float3 world_position : TEXCOORD0;
	float3 normal : NORMAL0;
	float2 uv : TEXCOORD1;
	float4 tangent : TANGENT0;
};

static const float PI = 3.1415926535897932384626433832795;

//ALBEDO in the GLSL: the albedo texture, converted to linear
float3 sample_albedo(float2 uv) {
	return pow(zest_textures[NonUniformResourceIndex(albedo_index)].Sample(zest_samplers[NonUniformResourceIndex(sampler_index)], uv).rgb, float3(2.2, 2.2, 2.2));
}

//GLSL constructors fill columns and HLSL constructors fill rows, so matrices built in the shader go through this
float3x3 columns3(float3 column0, float3 column1, float3 column2) {
	return transpose(float3x3(column0, column1, column2));
}

//From http://filmicgames.com/archives/75
float3 uncharted2_tonemap(float3 x) {
	float A = 0.15;
	float B = 0.50;
	float C = 0.10;
	float D = 0.20;
	float E = 0.02;
	float F = 0.30;
	return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

//Normal distribution function
float d_ggx(float dot_nh, float roughness) {
	float alpha = roughness * roughness;
	float alpha2 = alpha * alpha;
	float denom = dot_nh * dot_nh * (alpha2 - 1.0) + 1.0;
	return (alpha2) / (PI * denom * denom);
}

//Geometric shadowing function
float g_schlicksmith_ggx(float dot_nl, float dot_nv, float roughness) {
	float r = (roughness + 1.0);
	float k = (r * r) / 8.0;
	float geometry_light = dot_nl / (dot_nl * (1.0 - k) + k);
	float geometry_view = dot_nv / (dot_nv * (1.0 - k) + k);
	return geometry_light * geometry_view;
}

//Fresnel function
float3 f_schlick(float cos_theta, float3 F0) {
	return F0 + (1.0 - F0) * pow(1.0 - cos_theta, 5.0);
}

float3 f_schlick_r(float cos_theta, float3 F0, float roughness) {
	return F0 + (max((1.0 - roughness).xxx, F0) - F0) * pow(1.0 - cos_theta, 5.0);
}

float3 prefiltered_reflection(float3 R, float roughness) {
	const float MAX_REFLECTION_LOD = 9.0;
	float lod = roughness * MAX_REFLECTION_LOD;
	float lodf = floor(lod);
	float lodc = ceil(lod);
	float3 a = zest_cubemaps[NonUniformResourceIndex(pre_filtered_index)].SampleLevel(zest_samplers[NonUniformResourceIndex(sampler_index)], R, lodf).rgb;
	float3 b = zest_cubemaps[NonUniformResourceIndex(pre_filtered_index)].SampleLevel(zest_samplers[NonUniformResourceIndex(sampler_index)], R, lodc).rgb;
	return lerp(a, b, lod - lodf);
}

//The albedo sample sits inside the dot_nl branch, as in the GLSL, so its derivatives are taken in divergent flow on both backends
float3 specular_contribution(float3 L, float3 V, float3 N, float3 F0, float metallic, float roughness, float2 uv) {
	//Precalculate vectors and dot products
	float3 H = normalize(V + L);
	float dot_nh = clamp(dot(N, H), 0.0, 1.0);
	float dot_nv = clamp(dot(N, V), 0.0, 1.0);
	float dot_nl = clamp(dot(N, L), 0.0, 1.0);

	float3 contribution = float3(0.0, 0.0, 0.0);

	if (dot_nl > 0.0) {
		//D = Normal distribution (Distribution of the microfacets)
		float D = d_ggx(dot_nh, roughness);
		//G = Geometric shadowing term (Microfacets shadowing)
		float G = g_schlicksmith_ggx(dot_nl, dot_nv, roughness);
		//F = Fresnel factor (Reflectance depending on angle of incidence)
		float3 F = f_schlick(dot_nv, F0);
		float3 spec = D * F * G / (4.0 * dot_nl * dot_nv + 0.001);
		float3 kD = (float3(1.0, 1.0, 1.0) - F) * (1.0 - metallic);
		contribution += (kD * sample_albedo(uv) / PI + spec) * dot_nl;
	}

	return contribution;
}

float3 calculate_normal(varyings input) {
	float3 tangent_normal = zest_textures[NonUniformResourceIndex(normal_index)].Sample(zest_samplers[NonUniformResourceIndex(sampler_index)], input.uv).xyz * 2.0 - 1.0;

	float3 N = normalize(input.normal);
	float3 T = normalize(input.tangent.xyz);
	float3 B = normalize(cross(N, T));
	float3x3 TBN = columns3(T, B, N);
	return normalize(mul(TBN, tangent_normal));
}

float4 main(varyings input) : SV_Target0 {
	lights_t ubo_lights = zest_uniforms[NonUniformResourceIndex(lights_buffer_index)];
	float3 N = calculate_normal(input);

	float3 V = normalize(camera.xyz - input.world_position);
	float3 R = reflect(-V, N);

	float metallic = zest_textures[NonUniformResourceIndex(metallic_index)].Sample(zest_samplers[NonUniformResourceIndex(sampler_index)], input.uv).r;
	float roughness = zest_textures[NonUniformResourceIndex(roughness_index)].Sample(zest_samplers[NonUniformResourceIndex(sampler_index)], input.uv).r;

	float3 F0 = float3(0.04, 0.04, 0.04);
	F0 = lerp(F0, sample_albedo(input.uv), metallic);

	//The GLSL's loop bound is the length of a vec4, so 4
	float3 Lo = float3(0.0, 0.0, 0.0);
	for (int light_index = 0; light_index != 4; ++light_index) {
		float3 L = normalize(ubo_lights.lights[light_index].xyz - input.world_position);
		Lo += specular_contribution(L, V, N, F0, metallic, roughness, input.uv);
	}

	float2 brdf = zest_textures[NonUniformResourceIndex(brd_lookup_index)].Sample(zest_samplers[NonUniformResourceIndex(sampler_index)], float2(max(dot(N, V), 0.0), roughness)).rg;
	float3 reflection = prefiltered_reflection(R, roughness).rgb;
	float3 irradiance = zest_cubemaps[NonUniformResourceIndex(irradiance_index)].Sample(zest_samplers[NonUniformResourceIndex(sampler_index)], N).rgb;

	//Diffuse based on irradiance
	float3 diffuse = irradiance * sample_albedo(input.uv);

	float3 F = f_schlick_r(max(dot(N, V), 0.0), F0, roughness);

	//Specular reflectance
	float3 specular = reflection * (F * brdf.x + brdf.y);

	//Ambient part
	float3 kD = 1.0 - F;
	kD *= 1.0 - metallic;
	float3 ambient = (kD * diffuse + specular) * zest_textures[NonUniformResourceIndex(ao_index)].Sample(zest_samplers[NonUniformResourceIndex(sampler_index)], input.uv).rrr;

	float3 color = ambient + Lo;

	//Tone mapping
	color = uncharted2_tonemap(color * ubo_lights.exposure);
	color = color * (1.0 / uncharted2_tonemap(float3(11.2, 11.2, 11.2)));
	//Gamma correction
	color = pow(color, (1.0 / ubo_lights.gamma).xxx);

	return float4(color, 1.0);
}
