#version 450

layout (set = 0, binding = 0) uniform sampler2D samplerColor; 

layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 outColor;

layout(push_constant) uniform frag_pushes
{
    vec4 threshold_settings; // xyz for threshold, w for feather/softness (optional)
} settings;

float luminance(vec3 color) {
    return dot(color, vec3(0.2126, 0.7152, 0.0722));
}

void main(void)
{
    outColor = texture(samplerColor, inUV);
    /*
    vec4 source_color = texture(samplerColor, inUV); 

    float brightness = luminance(source_color.rgb);
    float threshold_value = settings.threshold_settings.x; // Main threshold
    float feather = settings.threshold_settings.y;            // Softness of the threshold (e.g., 0.1 to 0.5)

    float soft_factor = clamp((brightness - (threshold_value - feather)) / (2.0 * feather + 0.00001), 0.0, 1.0); 

    vec3 filtered_color = source_color.rgb * soft_factor;
    */

    /*
    if (brightness < threshold_value - feather) { 
        outColor = vec4(0.0, 0.0, 0.0, source_color.a);
    } else {
        outColor = vec4(filtered_color, source_color.a);
    }
    */
}