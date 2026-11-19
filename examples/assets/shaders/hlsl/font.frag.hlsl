cbuffer zest_push : register(b0, space0) {
	float4 transform;
	float4 shadow_color;
	float2 shadow_offset;		//In screen pixels
	float2 unit_range;
	float in_bias;
	float out_bias;
	float smoothness;
	float gamma;
	uint sampler_index;
	uint image_index;
	uint binding_number;		//1 for Texture2D, 3 for Texture2DArray. Changes if the font texture is on more than one layer
};

SamplerState zest_samplers[] : register(s0, space100);
Texture2D zest_textures[] : register(t0, space101);
Texture2DArray zest_texture_arrays[] : register(t0, space103);

struct varyings {
	float4 position : SV_Position;
	float3 tex_coord : TEXCOORD0;
	float4 color : COLOR0;
};

float median(float r, float g, float b) {
	return max(min(r, g), min(max(r, g), b));
}

float screen_pixel_range(float2 tex_coord) {
	float2 screen_texture_size = float2(1.0, 1.0) / fwidth(tex_coord);
	return max(0.5 * dot(unit_range, screen_texture_size), 1.0);
}

float contour(float distance, float2 tex_coord) {
	float width = screen_pixel_range(tex_coord);
	float edge = width * (distance - 0.5 + in_bias) + 0.5 + out_bias;
	return lerp(clamp(edge, 0.0, 1.0), smoothstep(0.0, 1.0, edge), smoothness);
}

float3 sample_font_texture(float2 uv, float layer) {
	if (binding_number == 1) {
		return zest_textures[NonUniformResourceIndex(image_index)].Sample(zest_samplers[NonUniformResourceIndex(sampler_index)], uv).rgb;
	} else {
		return zest_texture_arrays[NonUniformResourceIndex(image_index)].Sample(zest_samplers[NonUniformResourceIndex(sampler_index)], float3(uv, layer)).rgb;
	}
}

float2 get_texture_size() {
	uint width;
	uint height;
	if (binding_number == 1) {
		zest_textures[NonUniformResourceIndex(image_index)].GetDimensions(width, height);
	} else {
		uint layer_count;
		zest_texture_arrays[NonUniformResourceIndex(image_index)].GetDimensions(width, height, layer_count);
	}
	return float2(width, height);
}

float4 main(varyings input) : SV_Target0 {
	//1. Calculate shadow alpha
	float2 texture_size = get_texture_size();
	float2 shadow_uv_offset = shadow_offset / texture_size;
	float3 shadow_msd = sample_font_texture(input.tex_coord.xy - shadow_uv_offset, input.tex_coord.z);
	float shadow_sd = median(shadow_msd.r, shadow_msd.g, shadow_msd.b);
	float shadow_alpha = contour(shadow_sd, input.tex_coord.xy) * shadow_color.a;

	//2. Calculate fill alpha
	float3 fill_msd = sample_font_texture(input.tex_coord.xy, input.tex_coord.z);
	float fill_sd = median(fill_msd.r, fill_msd.g, fill_msd.b);
	float fill_alpha = contour(fill_sd, input.tex_coord.xy);

	//3. Composite the text (foreground) over the shadow (background)
	float4 foreground = float4(input.color.rgb, fill_alpha);
	float4 background = float4(shadow_color.rgb, shadow_alpha);

	//Standard "A over B" alpha compositing
	float final_alpha = foreground.a + background.a * (1.0 - foreground.a);
	float3 final_rgb = (foreground.rgb * foreground.a + background.rgb * background.a * (1.0 - foreground.a)) / final_alpha;

	//Avoid division by zero
	if (final_alpha == 0.0) {
		final_rgb = float3(0.0, 0.0, 0.0);
	}

	//Apply gamma correction to the final combined shape
	final_alpha = pow(final_alpha, 1.0 / gamma);

	//Output pre-multiplied color for correct blending in the pipeline
	return float4(final_rgb * final_alpha, final_alpha);
}
