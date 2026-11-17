cbuffer zest_push : register(b0, space0) {
	uint write_buffer_index;
	uint verify_buffer_index;
	uint unused1;
	uint unused2;
	uint unused3;
};

RWTexture2D<float4> zest_images[] : register(u0, space106);

[numthreads(8, 8, 1)]
void main(uint3 thread_id : SV_DispatchThreadID) {
	zest_images[NonUniformResourceIndex(write_buffer_index)][thread_id.xy] = float4(1.0, 0.0, 0.0, 0.0);
}
