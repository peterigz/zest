import common;

extern struct Collider : ICollider;

struct PushConstants {
    uint buffer_index;
    uint count;
    uint unused1;
    uint unused2;
};

[[vk::binding(5, 0)]] RWStructuredBuffer<TestParticle> particles[];
[[vk::push_constant]] ConstantBuffer<PushConstants> push;

void simulate<TMotion : IMotion>(uint index) {
    if (index >= push.count) return;
    TestParticle particle;
    particle.velocity = float3(1.0, 1.0, 1.0);
    particle.position = TMotion.step(float3(float(index), 1.0, 2.0), particle.velocity);
    particle.flags = Collider.collide(particle.position) + COMMON_MARKER * 1000;
    particle.age = 0.5;
    particles[push.buffer_index][index] = particle;
}

[shader("compute")]
[numthreads(64, 1, 1)]
void simulate_a(uint3 thread_id : SV_DispatchThreadID) {
    simulate<MotionA>(thread_id.x);
}

[shader("compute")]
[numthreads(64, 1, 1)]
void simulate_b(uint3 thread_id : SV_DispatchThreadID) {
    simulate<MotionB>(thread_id.x);
}

[shader("compute")]
[numthreads(64, 1, 1)]
void simulate_generic<T : IMotion>(uint3 thread_id : SV_DispatchThreadID) {
    simulate<T>(thread_id.x);
}
