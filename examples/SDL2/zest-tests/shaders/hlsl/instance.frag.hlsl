struct instance_varyings {
	float4 position : SV_Position;
	float4 color : COLOR0;
};

float4 main(instance_varyings input) : SV_Target0 {
	return input.color;
}
