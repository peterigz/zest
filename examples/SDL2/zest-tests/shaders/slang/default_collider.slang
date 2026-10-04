import common;

struct DefaultCollider : ICollider {
    static uint collide(float3 position) { return 1; }
};

export struct Collider : ICollider = DefaultCollider;
