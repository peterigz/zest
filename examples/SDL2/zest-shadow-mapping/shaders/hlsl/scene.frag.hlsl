cbuffer zest_push : register(b0, space0) {
	uint sampler_index;
	uint shadow_index;
	uint view_uniform_index;
	uint offscreen_uniform_index;
	bool enable_pcf;
};

SamplerState zest_samplers[] : register(s0, space100);
Texture2D zest_textures[] : register(t0, space101);

struct varyings {
	float4 position : SV_Position;
	float3 normal : NORMAL0;
	float3 color : COLOR0;
	float3 view_vec : TEXCOORD0;
	float3 light_vec : TEXCOORD1;
	float4 shadow_coord : TEXCOORD2;
};

static const float ambient = 0.1;

//The shadow map is a depth texture, so .r is the stored depth, compared by hand
float texture_proj(float4 shadow_coord, float2 offset) {
	float shadow = 1.0;
	if (shadow_coord.z > -1.0 && shadow_coord.z < 1.0) {
		float dist = zest_textures[NonUniformResourceIndex(shadow_index)].Sample(zest_samplers[NonUniformResourceIndex(sampler_index)], shadow_coord.xy + offset).r;
		if (shadow_coord.w > 0.0 && dist < shadow_coord.z) {
			shadow = ambient;
		}
	}
	return shadow;
}

float filter_pcf(float4 shadow_coord) {
	uint texture_width;
	uint texture_height;
	zest_textures[NonUniformResourceIndex(shadow_index)].GetDimensions(texture_width, texture_height);
	float scale = 1.5;
	float dx = scale * 1.0 / float(texture_width);
	float dy = scale * 1.0 / float(texture_height);

	float shadow_factor = 0.0;
	int count = 0;
	int range = 1;

	for (int x = -range; x <= range; x++) {
		for (int y = -range; y <= range; y++) {
			shadow_factor += texture_proj(shadow_coord, float2(dx * x, dy * y));
			count++;
		}
	}
	return shadow_factor / count;
}

float4 main(varyings input) : SV_Target0 {
	float shadow = (enable_pcf == true) ? filter_pcf(input.shadow_coord / input.shadow_coord.w) : texture_proj(input.shadow_coord / input.shadow_coord.w, float2(0.0, 0.0));

	float3 N = normalize(input.normal);
	float3 L = normalize(input.light_vec);
	float3 diffuse = max(dot(N, L), ambient) * input.color;

	return float4(diffuse * shadow, 1.0);
}
