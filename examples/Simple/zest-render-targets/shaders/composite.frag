#version 450

layout(set = 0, binding = 0) uniform sampler2D base_sampler;
layout(set = 0, binding = 1) uniform sampler2D blend_sampler;

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 out_color;

// --- Blend Mode Functions (RGB components) ---

// Normal (Blend layer on top of Base, using Blend's alpha)
// Note: This isn't a function for RGB, it's how main() would handle it with mix.

// Multiply
vec3 blend_multiply(vec3 base, vec3 blend) {
    return base * blend;
}

// Screen
vec3 blend_screen(vec3 base, vec3 blend) {
    return vec3(1.0) - (vec3(1.0) - base) * (vec3(1.0) - blend);
}

// Overlay
// Combines Multiply and Screen. If base < 0.5, it darkens; otherwise, it lightens.
vec3 blend_overlay(vec3 base, vec3 blend) {
    vec3 result;
    // This can be done component-wise with a helper or direct ifs
    for (int i = 0; i < 3; ++i) {
        if (base[i] < 0.5) {
            result[i] = 2.0 * base[i] * blend[i];
        } else {
            result[i] = 1.0 - 2.0 * (1.0 - base[i]) * (1.0 - blend[i]);
        }
    }
    return result;
}

// Soft Light (Photoshop's formula)
// D(cs) function for Soft Light
float soft_light_D(float cs) {
    if (cs <= 0.25) {
        return (( (16.0 * cs - 12.0) * cs + 4.0) * cs);
    } else {
        return sqrt(cs);
    }
}
vec3 blend_soft_light(vec3 base, vec3 blend) {
    vec3 result;
    for (int i = 0; i < 3; ++i) {
        if (blend[i] < 0.5) {
            result[i] = base[i] - (1.0 - 2.0 * blend[i]) * base[i] * (1.0 - base[i]);
        } else {
            result[i] = base[i] + (2.0 * blend[i] - 1.0) * (soft_light_D(base[i]) - base[i]);
        }
    }
    return result;
}

// Hard Light
// Swaps roles of base and blend for the condition compared to Overlay.
// If blend color < 0.5, it multiplies; otherwise, it screens.
vec3 blend_hard_light(vec3 base, vec3 blend) {
    vec3 result;
    for (int i = 0; i < 3; ++i) {
        if (blend[i] < 0.5) {
            result[i] = 2.0 * base[i] * blend[i]; // Multiply
        } else {
            result[i] = 1.0 - 2.0 * (1.0 - base[i]) * (1.0 - blend[i]); // Screen
        }
    }
    return result;
}

// Linear Dodge (Add)
// Good for glows, sparks, etc. Works naturally with HDR values too.
vec3 blend_add(vec3 base, vec3 blend) {
    return base + blend; // For LDR, you might clamp(base + blend, 0.0, 1.0);
}

// Difference
vec3 blend_difference(vec3 base, vec3 blend) {
    return abs(base - blend);
}

// Exclusion
// Similar to Difference but lower contrast.
vec3 blend_exclusion(vec3 base, vec3 blend) {
    return base + blend - 2.0 * base * blend;
}

// Lighten
// Selects the lighter of the base or blend color for each component.
vec3 blend_lighten(vec3 base, vec3 blend) {
    return max(base, blend);
}

// Darken
// Selects the darker of the base or blend color for each component.
vec3 blend_darken(vec3 base, vec3 blend) {
    return min(base, blend);
}

// Color Dodge
// Brightens the base color to reflect the blend color.
// Division by zero if blend component is 1.0; result should be white.
vec3 blend_color_dodge(vec3 base, vec3 blend) {
    vec3 result = vec3(0.0);
    if (blend.r < 1.0) result.r = clamp(base.r / (1.0 - blend.r), 0.0, 1.0); else result.r = 1.0;
    if (blend.g < 1.0) result.g = clamp(base.g / (1.0 - blend.g), 0.0, 1.0); else result.g = 1.0;
    if (blend.b < 1.0) result.b = clamp(base.b / (1.0 - blend.b), 0.0, 1.0); else result.b = 1.0;
    return result;
}

// Color Burn
// Darkens the base color to reflect the blend color.
// Division by zero if blend component is 0.0; result should be black.
vec3 blend_color_burn(vec3 base, vec3 blend) {
    vec3 result = vec3(1.0);
    if (blend.r > 0.0) result.r = clamp(1.0 - (1.0 - base.r) / blend.r, 0.0, 1.0); else result.r = 0.0;
    if (blend.g > 0.0) result.g = clamp(1.0 - (1.0 - base.g) / blend.g, 0.0, 1.0); else result.g = 0.0;
    if (blend.b > 0.0) result.b = clamp(1.0 - (1.0 - base.b) / blend.b, 0.0, 1.0); else result.b = 0.0;
    return result;
}

void main(void)
{
    vec4 base_color = texture(base_sampler, inUV);
    vec4 blend_color = texture(blend_sampler, inUV) * 2.0;
    blend_color = clamp(blend_color, 0.0, 1.0);

    vec3 blended_rgb;

        // --- CHOOSE YOUR BLEND MODE TO EXPERIMENT ---
    // blended_rgb = base_color.rgb; // No blend, just show base
    // blended_rgb = blend_color.rgb; // No blend, just show blend layer (useful for checking its content)

    // Separable Blend Modes (operate on RGB independently)
    //blended_rgb = blend_multiply(base_color.rgb, blend_color.rgb);
    //blended_rgb = blend_screen(base_color.rgb, blend_color.rgb);
    //blended_rgb = blend_overlay(base_color.rgb, blend_color.rgb);
    //blended_rgb = blend_soft_light(base_color.rgb, blend_color.rgb);
    //blended_rgb = blend_hard_light(base_color.rgb, blend_color.rgb);
    blended_rgb = blend_add(base_color.rgb, blend_color.rgb); // Good starting point for effects like blur/bloom
    //blended_rgb = blend_difference(base_color.rgb, blend_color.rgb);
    //blended_rgb = blend_exclusion(base_color.rgb, blend_color.rgb);
    //blended_rgb = blend_lighten(base_color.rgb, blend_color.rgb);
    //blended_rgb = blend_darken(base_color.rgb, blend_color.rgb);
    //blended_rgb = blend_color_dodge(base_color.rgb, blend_color.rgb);
    //blended_rgb = blend_color_burn(base_color.rgb, blend_color.rgb);

    // --- Alpha Handling & Output ---
    // Option 1: Use blend layer's alpha to mix the blended RGB with the base RGB.
    // This is good if blend_color represents an effect you want to fade in/out.
    // The final alpha could be the base's alpha, or max of both, or blend_color.a if it's an overlay.
    float blend_amount = 1.0; // Assuming blend_color.a controls the opacity of the blend effect
    // If you have a global opacity uniform: blend_amount *= u_blendOpacity;
    blend_amount *= 0.75;

    out_color.rgb = mix(base_color.rgb, blended_rgb, blend_amount);
    out_color.a = base_color.a; // Preserve base alpha, or choose another strategy like max(base_color.a, blend_color.a)

    // Option 2: If both inputs are considered opaque and you just want the RGB result.
    //out_color = vec4(blended_rgb, 1.0);

    // Option 3: Full "Over" Porter-Duff compositing (if blend_color itself has transparency over base_color)
    // This treats blended_rgb as the color of the blend layer if it were fully opaque.
    // float outA = blend_color.a + base_color.a * (1.0 - blend_color.a);
    // if (outA > 0.0001) { // Avoid division by zero
    //     out_color.rgb = (blended_rgb * blend_color.a + base_color.rgb * base_color.a * (1.0 - blend_color.a)) / outA;
    // } else {
    //     out_color.rgb = vec3(0.0);
    // }
    // out_color.a = outA;
}



