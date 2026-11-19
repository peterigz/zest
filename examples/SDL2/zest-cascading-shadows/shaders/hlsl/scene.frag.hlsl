cbuffer zest_push : register(b0, space0) {
	uint sampler_index;
	uint shadow_sampler_index;
	uint shadow_index;
	uint vert_index;
	uint frag_index;
	uint cascade_index;
	uint texture_index;
	uint display_cascade_index;
	int enable_pcf;
};

#define SHADOW_MAP_CASCADE_COUNT 4

struct fragment_view_t {
	float4 cascade_splits;
	float4x4 inverse_view_mat;
	float3 light_dir;
	float pad;
	int color_cascades;
};

struct cascade_matrices_t {
	float4x4 matrices[SHADOW_MAP_CASCADE_COUNT];
};

SamplerState zest_samplers[] : register(s0, space100);
Texture2D zest_textures[] : register(t0, space101);
Texture2DArray zest_texture_arrays[] : register(t0, space103);
//Two block types read from the uniform buffer binding, so the second takes space 200 + binding
ConstantBuffer<fragment_view_t> zest_uniforms[] : register(b0, space107);
ConstantBuffer<cascade_matrices_t> zest_cascade_uniforms[] : register(b0, space207);

struct varyings {
	float4 position : SV_Position;
	float3 normal : NORMAL0;
	float3 color : COLOR0;
	float3 view_position : TEXCOORD0;
	float3 world_position : TEXCOORD1;
	float3 uv : TEXCOORD2;
};

static const float ambient = 0.3;

//GLSL constructors fill columns and HLSL constructors fill rows, so matrices built in the shader go through this
float4x4 columns4(float4 column0, float4 column1, float4 column2, float4 column3) {
	return transpose(float4x4(column0, column1, column2, column3));
}

float texture_proj(float4 shadow_coord, float2 offset, uint cascade) {
	float shadow = 1.0;
	float bias = 0.005;

	if (shadow_coord.z > -1.0 && shadow_coord.z < 1.0) {
		float dist = zest_texture_arrays[NonUniformResourceIndex(shadow_index)].Sample(zest_samplers[NonUniformResourceIndex(shadow_sampler_index)], float3(shadow_coord.xy + offset, cascade)).r;
		if (shadow_coord.w > 0 && dist < shadow_coord.z - bias) {
			shadow = ambient;
		}
	}
	return shadow;
}

float filter_pcf(float4 shadow_coord, uint cascade) {
	int2 texture_dimensions = int2(2048, 2048);
	float scale = 0.75;
	float dx = scale * 1.0 / float(texture_dimensions.x);
	float dy = scale * 1.0 / float(texture_dimensions.y);

	float shadow_factor = 0.0;
	int count = 0;
	int range = 1;

	for (int x = -range; x <= range; x++) {
		for (int y = -range; y <= range; y++) {
			shadow_factor += texture_proj(shadow_coord, float2(dx * x, dy * y), cascade);
			count++;
		}
	}
	return shadow_factor / count;
}

float4 main(varyings input) : SV_Target0 {
	fragment_view_t ubo = zest_uniforms[NonUniformResourceIndex(frag_index)];
	//Maps clip space x and y from -1..1 to texture coordinates 0..1
	float4x4 bias_mat = columns4(float4(0.5, 0.0, 0.0, 0.0), float4(0.0, 0.5, 0.0, 0.0), float4(0.0, 0.0, 1.0, 0.0), float4(0.5, 0.5, 0.0, 1.0));

	float4 color = zest_textures[NonUniformResourceIndex(texture_index)].Sample(zest_samplers[NonUniformResourceIndex(sampler_index)], input.uv.xy);
	if (color.a < 0.5) {
		discard;
	}

	//Get cascade index for the current fragment's view position
	uint cascade = 0;
	for (uint split_index = 0; split_index < SHADOW_MAP_CASCADE_COUNT - 1; ++split_index) {
		if (input.view_position.z < ubo.cascade_splits[split_index]) {
			cascade = split_index + 1;
		}
	}

	//Depth compare for shadowing
	float4 shadow_coord = mul(mul(bias_mat, zest_cascade_uniforms[NonUniformResourceIndex(cascade_index)].matrices[cascade]), float4(input.world_position, 1.0));

	float shadow = 0;
	if (enable_pcf == 1) {
		shadow = filter_pcf(shadow_coord / shadow_coord.w, cascade);
	} else {
		shadow = texture_proj(shadow_coord / shadow_coord.w, float2(0.0, 0.0), cascade);
	}

	//Directional light
	float3 N = normalize(input.normal);
	float3 L = normalize(-ubo.light_dir);
	float diffuse = max(dot(N, L), ambient);
	float3 light_color = float3(1.0, 1.0, 1.0);
	float4 out_color;
	out_color.rgb = max(light_color * (diffuse * color.rgb), float3(0.0, 0.0, 0.0));
	out_color.rgb *= shadow;
	out_color.a = color.a;

	//Color cascades (if enabled)
	if (ubo.color_cascades == 1) {
		switch (cascade) {
			case 0:
				out_color.rgb *= float3(1.0, 0.25, 0.25);
				break;
			case 1:
				out_color.rgb *= float3(0.25, 1.0, 0.25);
				break;
			case 2:
				out_color.rgb *= float3(0.25, 0.25, 1.0);
				break;
			case 3:
				out_color.rgb *= float3(1.0, 1.0, 0.25);
				break;
		}
	}
	return out_color;
}
