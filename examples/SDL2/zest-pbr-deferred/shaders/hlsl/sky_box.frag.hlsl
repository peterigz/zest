cbuffer zest_push : register(b0, space0) {
	uint view_index;
	uint lights_index;
};

struct params_t {
	float4 lights[4];
	float exposure;
	float gamma;
	uint texture_index;
	uint sampler_index;
};

SamplerState zest_samplers[] : register(s0, space100);
TextureCube zest_cubemaps[] : register(t0, space102);
ConstantBuffer<params_t> zest_uniforms[] : register(b0, space107);

struct varyings {
	float4 position : SV_Position;
	float3 uvw : TEXCOORD0;
};

//From http://filmicworlds.com/blog/filmic-tonemapping-operators/
float3 uncharted2_tonemap(float3 color) {
	float A = 0.15;
	float B = 0.50;
	float C = 0.10;
	float D = 0.20;
	float E = 0.02;
	float F = 0.30;
	return ((color * (A * color + C * B) + D * E) / (color * (A * color + B) + D * F)) - E / F;
}

float4 main(varyings input) : SV_Target0 {
	params_t consts = zest_uniforms[NonUniformResourceIndex(lights_index)];
	float3 color = zest_cubemaps[NonUniformResourceIndex(consts.texture_index)].SampleLevel(zest_samplers[NonUniformResourceIndex(consts.sampler_index)], input.uvw, 0.0).rgb;

	//Tone mapping
	color = uncharted2_tonemap(color * consts.exposure);
	color = color * (1.0 / uncharted2_tonemap(float3(11.2, 11.2, 11.2)));
	//Gamma correction
	color = pow(color, (1.0 / consts.gamma).xxx);

	return float4(color, 1.0);
}
