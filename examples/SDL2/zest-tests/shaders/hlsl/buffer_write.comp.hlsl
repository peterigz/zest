cbuffer zest_push : register(b0, space0) {
	uint write_buffer_index;
	uint verify_buffer_index;
	uint unused1;
	uint unused2;
	uint unused3;
};

RWByteAddressBuffer zest_buffers[] : register(u0, space105);

[numthreads(8, 8, 1)]
void main(uint3 thread_id : SV_DispatchThreadID) {
	uint index = thread_id.x;
	zest_buffers[NonUniformResourceIndex(write_buffer_index)].Store4(index * 16, asuint(float4(float(index), 0.0, 0.0, 0.0)));
}
