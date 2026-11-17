cbuffer zest_push : register(b0, space0) {
	uint buffer_index;
	uint result_index;
	uint word_count;
	uint seed;
	uint mode;
};

//The result is two uints: words checked, then mismatches
RWByteAddressBuffer zest_buffers[] : register(u0, space105);

[numthreads(64, 1, 1)]
void main(uint3 thread_id : SV_DispatchThreadID) {
	uint index = thread_id.x;
	if (index >= word_count) {
		return;
	}
	if (mode == 0) {
		zest_buffers[NonUniformResourceIndex(buffer_index)].Store(index * 4, seed + index);
	} else {
		zest_buffers[NonUniformResourceIndex(result_index)].InterlockedAdd(0, 1);
		if (zest_buffers[NonUniformResourceIndex(buffer_index)].Load(index * 4) != seed + index) {
			zest_buffers[NonUniformResourceIndex(result_index)].InterlockedAdd(4, 1);
		}
	}
}
