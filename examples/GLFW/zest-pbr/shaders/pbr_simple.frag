#version 450

layout(location = 0) in vec4 in_frag_color;
layout (location = 1) in vec3 in_world_position;
layout (location = 2) in vec3 in_normal;

layout(location = 0) out vec4 out_color;

layout(push_constant) uniform quad_index
{
    uint index1;
    uint index2;
    uint index3;
    uint index4;
    float roughness;
	float metallic;
	vec2 padding1;
	vec3 color;
	float padding2;
	vec4 parameters3;
	vec4 camera;
} material;

layout (set = 1, binding = 0) uniform UBOLights {
	vec4 lights[4];
	float exposure;
	float gamma;
} ubo_lights;

const float PI = 3.14159265359;

//#define ROUGHNESS_PATTERN 1

vec3 materialcolor()
{
	return vec3(material.color.r, material.color.g, material.color.b);
}

// Normal Distribution function --------------------------------------
float D_GGX(float dotNH, float roughness)
{
	float alpha = roughness * roughness;
	float alpha2 = alpha * alpha;
	float denom = dotNH * dotNH * (alpha2 - 1.0) + 1.0;
	return (alpha2)/(PI * denom*denom); 
}

// Geometric Shadowing function --------------------------------------
float G_SchlicksmithGGX(float dotNL, float dotNV, float roughness)
{
	float r = (roughness + 1.0);
	float k = (r*r) / 8.0;
	float GL = dotNL / (dotNL * (1.0 - k) + k);
	float GV = dotNV / (dotNV * (1.0 - k) + k);
	return GL * GV;
}

// Fresnel function ----------------------------------------------------
vec3 F_Schlick(float cosTheta, float metallic)
{
	vec3 F0 = mix(vec3(0.04), materialcolor(), metallic); // * material.specular
	vec3 F = F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0); 
	return F;    
}

// Specular BRDF composition --------------------------------------------

vec3 BRDF(vec3 L, vec3 V, vec3 N, float metallic, float roughness)
{
	// Precalculate vectors and dot products	
	vec3 H = normalize (V + L);
	float dotNV = clamp(dot(N, V), 0.0, 1.0);
	float dotNL = clamp(dot(N, L), 0.0, 1.0);
	float dotLH = clamp(dot(L, H), 0.0, 1.0);
	float dotNH = clamp(dot(N, H), 0.0, 1.0);

	// Light color fixed
	vec3 lightColor = vec3(1.0);

	vec3 color = vec3(0.0);

	if (dotNL > 0.0)
	{
		float rroughness = max(0.05, roughness);
		// D = Normal distribution (Distribution of the microfacets)
		float D = D_GGX(dotNH, roughness); 
		// G = Geometric shadowing term (Microfacets shadowing)
		float G = G_SchlicksmithGGX(dotNL, dotNV, rroughness);
		// F = Fresnel factor (Reflectance depending on angle of incidence)
		vec3 F = F_Schlick(dotNV, metallic);

		vec3 spec = D * F * G / (4.0 * dotNL * dotNV);

		color += spec * dotNL * lightColor;
	}

	return color;
}

// ----------------------------------------------------------------------------
void main()
{		  
	vec3 N = normalize(in_normal);
	vec3 V = normalize(material.camera.xyz - in_world_position);

	float roughness = material.roughness;

	// Add striped pattern to roughness based on vertex position
#ifdef ROUGHNESS_PATTERN
	roughness = max(roughness, step(fract(in_world_position.y * 2.02), 0.5));
#endif

	// Specular contribution
	vec3 Lo = vec3(0.0);
	for (int i = 0; i < ubo_lights.lights.length(); i++) {
		vec3 L = normalize(ubo_lights.lights[i].xyz - in_world_position);
		Lo += BRDF(L, V, N, material.metallic, roughness);
	};

	// Combine with ambient
	vec3 color = materialcolor() * 0.02;
	color += Lo;

	// Gamma correct
	color = pow(color, vec3(0.4545));

	out_color = vec4(color, 1.0);
}
