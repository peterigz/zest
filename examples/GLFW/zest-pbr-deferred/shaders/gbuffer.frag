#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 in_world_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec4 in_color;
layout(location = 3) in vec2 in_pbr;

// Multiple Render Targets (G-buffer outputs)
layout(location = 0) out vec4 gPosition;    // World position XYZ
layout(location = 1) out vec4 gNormal;      // World normal (packed to [0,1])
layout(location = 2) out vec4 gAlbedo;      // Albedo RGB
layout(location = 3) out vec4 gPBR;         // Roughness, Metallic, AO, unused

void main() {
	// Output world position
	gPosition = vec4(in_world_position, 1.0);

	// Output normal packed to [0,1] range for storage
	gNormal = vec4(normalize(in_normal) * 0.5 + 0.5, 1.0);

	// Output albedo color
	gAlbedo = vec4(in_color.rgb, 1.0);

	// Output PBR properties: roughness, metallic, AO (default 1.0), unused
	gPBR = vec4(in_pbr.x, in_pbr.y, 1.0, 1.0);
}
