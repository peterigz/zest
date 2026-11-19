cbuffer zest_push : register(b0, space0) {
	uint sampler_index;
	uint planet_index;
	uint rock_index;
	uint skybox_index;
	uint ubo_index;
};

SamplerState zest_samplers[] : register(s0, space100);
Texture2D zest_textures[] : register(t0, space101);

struct varyings {
	float4 position : SV_Position;
	float3 normal : NORMAL0;
	float3 color : COLOR0;
	float2 uv : TEXCOORD0;
	float3 view_vec : TEXCOORD1;
	float3 light_vec : TEXCOORD2;
};

float4 main(varyings input) : SV_Target0 {
	float4 color = zest_textures[NonUniformResourceIndex(planet_index)].Sample(zest_samplers[NonUniformResourceIndex(sampler_index)], input.uv) * float4(input.color, 1.0) * 1.5;
	float3 N = normalize(input.normal);
	float3 L = normalize(input.light_vec);
	float3 V = normalize(input.view_vec);
	float3 R = reflect(-L, N);
	float3 diffuse = max(dot(N, L), 0.0) * input.color;
	float3 specular = pow(max(dot(R, V), 0.0), 4.0) * float3(0.5, 0.5, 0.5) * color.r;
	return float4(diffuse * color.rgb + specular, 1.0);
}
