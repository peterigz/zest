import common;

struct UserCollider : ICollider {
    static uint collide(float3 position) { return uint(position.x) + 100; }
};

export struct Collider : ICollider = UserCollider;
