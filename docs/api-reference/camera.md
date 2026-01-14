# Camera API

Camera utilities and frustum culling.

## Camera Creation

### zest_CreateCamera

```cpp
zest_camera zest_CreateCamera(void);
```

---

## Camera Movement

### zest_CameraMoveForward / zest_CameraMoveBackward

```cpp
void zest_CameraMoveForward(zest_camera *camera, float speed);
void zest_CameraMoveBackward(zest_camera *camera, float speed);
```

---

### zest_CameraMoveUp / zest_CameraMoveDown

```cpp
void zest_CameraMoveUp(zest_camera *camera, float speed);
void zest_CameraMoveDown(zest_camera *camera, float speed);
```

---

### zest_CameraStrafLeft / zest_CameraStrafRight

```cpp
void zest_CameraStrafLeft(zest_camera *camera, float speed);
void zest_CameraStrafRight(zest_camera *camera, float speed);
```

---

## Camera Rotation

### zest_TurnCamera

```cpp
void zest_TurnCamera(zest_camera *camera, float yaw_delta, float pitch_delta);
```

---

### zest_CameraUpdateFront

Recalculate front vector from yaw/pitch.

```cpp
void zest_CameraUpdateFront(zest_camera *camera);
```

---

## Camera Setup

### zest_CameraPosition

```cpp
void zest_CameraPosition(zest_camera *camera, zest_vec3 position);
```

---

### zest_CameraSetFoV

```cpp
void zest_CameraSetFoV(zest_camera *camera, float fov);
```

---

### zest_CameraSetPitch / zest_CameraSetYaw

```cpp
void zest_CameraSetPitch(zest_camera *camera, float pitch);
void zest_CameraSetYaw(zest_camera *camera, float yaw);
```

---

## Screen/World Conversion

### zest_ScreenRay

Get ray from screen position.

```cpp
zest_vec3 zest_ScreenRay(
    zest_camera *camera,
    float screen_x,
    float screen_y,
    float screen_width,
    float screen_height
);
```

---

### zest_WorldToScreen

```cpp
zest_vec2 zest_WorldToScreen(
    zest_camera *camera,
    zest_vec3 world_pos,
    float screen_width,
    float screen_height
);
```

---

### zest_WorldToScreenOrtho

```cpp
zest_vec2 zest_WorldToScreenOrtho(
    zest_matrix4 *projection,
    zest_vec3 world_pos,
    float screen_width,
    float screen_height
);
```

---

## Frustum Culling

### zest_CalculateFrustumPlanes

```cpp
void zest_CalculateFrustumPlanes(zest_camera *camera, zest_vec4 planes[6]);
```

---

### zest_IsPointInFrustum

```cpp
zest_bool zest_IsPointInFrustum(zest_vec4 planes[6], zest_vec3 point);
```

---

### zest_IsSphereInFrustum

```cpp
zest_bool zest_IsSphereInFrustum(zest_vec4 planes[6], zest_vec3 center, float radius);
```

---

### zest_RayIntersectPlane

```cpp
zest_bool zest_RayIntersectPlane(
    zest_vec3 ray_origin,
    zest_vec3 ray_dir,
    zest_vec4 plane,
    float *t
);
```

---

## See Also

- [Math API](math.md)
