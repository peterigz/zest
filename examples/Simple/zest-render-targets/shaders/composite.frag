#version 450

layout(set = 0, binding = 0) uniform sampler2D base_sampler;
layout(set = 0, binding = 1) uniform sampler2D blend_sampler;

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 out_color;

// Good for glows, sparks, etc. Works naturally with HDR values too.
vec3 blend_add(vec3 base, vec3 blend) {
    return base + blend; // For LDR, you might clamp(base + blend, 0.0, 1.0);
}

vec3 blend_screen(vec3 base, vec3 blend) {
    return vec3(1.0) - (vec3(1.0) - base) * (vec3(1.0) - blend);
}

layout(push_constant) uniform frag_pushes
{
    // x: exposure
    // y: Tonemapper choice (e.g., 0 for Reinhard, 1 for Uncharted 2, 2 for ACES Filmic (simplified))
    // z: Unused
    // w: (Optional) White point for Uncharted 2 / ACES
    vec4 tonemapping_params;
    // x: blend amount
    vec4 composite_settings;
} settings;

vec3 tonemap_reinhard(vec3 color, float exposure) {
    color *= exposure;
    return color / (color + vec3(1.0));
}

// Constants for Uncharted 2 operator
const float A = 0.15; // Shoulder Strength
const float B = 0.50; // Linear Strength
const float C = 0.10; // Linear Angle
const float D = 0.20; // Toe Strength
const float E = 0.02; // Toe Numerator
const float F = 0.30; // Toe Denominator
// Note: E/F = Toe Angle

vec3 uncharted2TonemapPartial(vec3 color, float exposure) {
    color *= exposure;
    return ((color * (A * color + C * B) + D * E) / (color * (A * color + B) + D * F)) - E / F;
}

vec3 tonemap_uncharted2(vec3 color, float exposure, float whitePoint) {
    vec3 curr = uncharted2TonemapPartial(color, exposure);
    vec3 W_vector = vec3(whitePoint); // White point; linear value that should be mapped to 1.0
    vec3 white_scale = vec3(1.0f) / uncharted2TonemapPartial(W_vector, 1.0); // Use exposure=1.0 for W_vector as exposure is already applied to color
    return curr * white_scale;
}

// 3. ACES Filmic Tonemapping (Simplified)
// Approximates the ACES curve, known for good highlight handling and natural look.
// Source: https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
vec3 tonemap_aces_filmic(vec3 color, float exposure) {
    color *= exposure;
    const float a = 2.51f;
    const float b = 0.03f;
    const float c = 2.43f;
    const float d = 0.59f;
    const float e = 0.14f;
    return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
}

void main(void)
{
    //Composite the base and blend layers
    vec4 base_color = texture(base_sampler, inUV);
    vec4 blend_color = texture(blend_sampler, inUV) * 2.0;
    blend_color = clamp(blend_color, 0.0, 1.0);

    vec3 blended_rgb;

    blended_rgb = blend_add(base_color.rgb, blend_color.rgb); 

    float blend_amount = settings.composite_settings.x; 

	vec4 source_hdr_color = vec4(mix(base_color.rgb, blended_rgb, blend_amount), base_color.a); 

    //Setup for tonemapping
    vec3 linear_color = source_hdr_color.rgb;
    float alpha = source_hdr_color.a;

    float exposure = settings.tonemapping_params.x;
    if (exposure <= 0.0) {
        exposure = 1.0;
    }

    //Apply Tonemapping
    vec3 tonemapped_color;
    int tonemapper_choice = int(round(settings.tonemapping_params.y));
    float white_point = settings.tonemapping_params.w; // Optional, for Uncharted 2 / ACES like adjustments

    if (white_point <= 0.0) {
        white_point = 11.2; 
    }

    if (tonemapper_choice == 0) {
        tonemapped_color = tonemap_reinhard(linear_color, exposure);
    } else if (tonemapper_choice == 1) {
        tonemapped_color = tonemap_uncharted2(linear_color, exposure, white_point);
    } else if (tonemapper_choice == 2) {
        tonemapped_color = tonemap_aces_filmic(linear_color, exposure);
    } else {
        tonemapped_color = tonemap_reinhard(linear_color, exposure);
    }

    out_color = vec4(tonemapped_color, alpha);
}



