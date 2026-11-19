struct varyings {
	float4 position : SV_Position;
	float3 normal : NORMAL0;
	float4 color : COLOR0;
	float2 pbr : TEXCOORD0;
};

//Multiple render targets. Position is reconstructed from the depth buffer in the lighting pass
struct gbuffer_targets {
	float4 normal : SV_Target0;		//World normal, packed to [0,1]
	float4 albedo : SV_Target1;		//Albedo rgb
	float4 pbr : SV_Target2;		//Roughness, metallic, AO, unused
};

gbuffer_targets main(varyings input) {
	gbuffer_targets output;
	//Output normal packed to [0,1] range for storage
	output.normal = float4(normalize(input.normal) * 0.5 + 0.5, 1.0);

	//Output albedo color
	output.albedo = float4(input.color.rgb, 1.0);

	//Output PBR properties: roughness, metallic, AO (default 1.0), unused
	output.pbr = float4(input.pbr.x, input.pbr.y, 1.0, 1.0);
	return output;
}
