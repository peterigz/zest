struct TestParticle {
    float3 position;
    uint flags;
    float3 velocity;
    float age;
};

//32 bytes in std430, 24 with scalar layout or on D3D12
struct PackedPair {
    float3 first;
    float3 second;
};

//4 bytes on Vulkan and 8 on D3D12, so a type size shows which platform macro the session defined
struct PlatformMacro {
#if defined(ZEST_D3D12)
    uint values[2];
#elif defined(ZEST_VULKAN)
    uint values[1];
#endif
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
