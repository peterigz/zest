struct TestParticle {
    float3 position;
    uint flags;
    float3 velocity;
    float age;
};

//32 bytes in std430, 24 with scalar layout
struct PackedPair {
    float3 first;
    float3 second;
};

static const uint COMMON_MARKER = 1;

interface IMotion {
    static float3 step(float3 position, float3 velocity);
};

struct MotionA : IMotion {
    static float3 step(float3 position, float3 velocity) { return position + velocity; }
};

struct MotionB : IMotion {
    static float3 step(float3 position, float3 velocity) { return position + velocity * 2.0; }
};

interface ICollider {
    static uint collide(float3 position);
};
