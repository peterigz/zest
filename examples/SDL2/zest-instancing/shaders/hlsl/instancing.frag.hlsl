cbuffer zest_push : register(b0, space0) {
	uint sampler_index;
	uint planet_index;
	uint rock_index;
	uint skybox_index;
	uint ubo_index;
};

SamplerState zest_samplers[] : register(s0, space100);
Texture2DArray zest_texture_arrays[] : register(t0, space103);

struct varyings {
	float4 position : SV_Position;
	float3 normal : NORMAL0;
	float3 color : COLOR0;
	float3 uv : TEXCOORD0;
	float3 view_vec : TEXCOORD1;
	float3 light_vec : TEXCOORD2;
};

//uv.z is the texture array layer
float4 main(varyings input) : SV_Target0 {
	float4 color = zest_texture_arrays[NonUniformResourceIndex(rock_index)].Sample(zest_samplers[NonUniformResourceIndex(sampler_index)], input.uv) * float4(input.color, 1.0);
	float3 N = normalize(input.normal);
	float3 L = normalize(input.light_vec);
	float3 V = normalize(input.view_vec);
	float3 R = reflect(-L, N);
	float3 diffuse = max(dot(N, L), 0.1) * input.color;
	float3 specular = (dot(N, L) > 0.0) ? pow(max(dot(R, V), 0.0), 16.0) * float3(0.75, 0.75, 0.75) * color.r : float3(0.0, 0.0, 0.0);
	return float4(diffuse * color.rgb + specular, 1.0);
}
