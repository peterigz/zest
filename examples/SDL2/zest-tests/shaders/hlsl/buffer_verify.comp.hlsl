cbuffer zest_push : register(b0, space0) {
	uint write_buffer_index;
	uint verify_buffer_index;
	uint unused1;
	uint unused2;
	uint unused3;
};

ByteAddressBuffer zest_read_buffers[] : register(t0, space105);
RWByteAddressBuffer zest_buffers[] : register(u0, space105);

[numthreads(8, 8, 1)]
void main(uint3 thread_id : SV_DispatchThreadID) {
	uint index = thread_id.x;
	float value = asfloat(zest_read_buffers[NonUniformResourceIndex(write_buffer_index)].Load(index * 16));
	float success = value != float(index) ? 0.0 : 1.0;
	zest_buffers[NonUniformResourceIndex(verify_buffer_index)].Store(0, asuint(success));
}
