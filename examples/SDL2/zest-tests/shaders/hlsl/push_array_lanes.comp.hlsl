cbuffer zest_push : register(b0, space0) {
	uint output_index;
	uint3 padding;
	uint4 entries[4];
};

RWByteAddressBuffer zest_buffers[] : register(u0, space105);

//Lanes of one wave read different rows of entries, which root constants get wrong on AMD
[numthreads(64, 1, 1)]
void main(uint3 thread_id : SV_DispatchThreadID) {
	uint entry = thread_id.x & 15;
	zest_buffers[output_index].Store(thread_id.x * 4, entries[entry >> 2][entry & 3]);
}
