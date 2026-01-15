# Camera API

Zest provides some simple camera utilities and frustum culling. You can use these or create your own with your own libraries.

## Camera Creation

### zest_CreateCamera

```cpp
zest_camera_t zest_CreateCamera(void);
```

---

## Camera Movement

Moves the camera in a specific direction based on the speed value you pass in to the function.

### zest_CameraMoveForward / zest_CameraMoveBackward

```cpp
void zest_CameraMoveForward(zest_camera_t *camera, float speed);
void zest_CameraMoveBackward(zest_camera_t *camera, float speed);
```

---

### zest_CameraMoveUp / zest_CameraMoveDown

```cpp
void zest_CameraMoveUp(zest_camera_t *camera, float speed);
void zest_CameraMoveDown(zest_camera_t *camera, float speed);
```

---

### zest_CameraStrafLeft / zest_CameraStrafRight

```cpp
void zest_CameraStrafLeft(zest_camera_t *camera, float speed);
void zest_CameraStrafRight(zest_camera_t *camera, float speed);
```

---

## Camera Rotation

### zest_TurnCamera

Turn the camera by a given amount of degrees (gets converted to radians internally) scaled by the sensitivity that you pass into the function.

```cpp
void zest_TurnCamera(zest_camera_t *camera, float turn_x, float turn_y, float sensitivity);
```

---

### zest_CameraUpdateFront

Recalculate front vector from yaw/pitch.

```cpp
void zest_CameraUpdateFront(zest_camera_t *camera);
```

---

## Camera Setup

### zest_CameraPosition

Position the camera by passing in a float array of 3 elements.

```cpp
void zest_CameraPosition(zest_camera_t *camera, float position[3]);
```

---

### zest_CameraSetFoV

Sets the Field of View for the camera in degrees.

```cpp
void zest_CameraSetFoV(zest_camera_t *camera, float fov);
```

---

### zest_CameraSetPitch / zest_CameraSetYaw

Set the pitch and yaw of the camera in degrees.

```cpp
void zest_CameraSetPitch(zest_camera_t *camera, float pitch);
void zest_CameraSetYaw(zest_camera_t *camera, float yaw);
```

---

## Screen/World Conversion

### zest_ScreenRay

Get ray from screen position. You pass in the screen pixel coordinates (xpos and ypos), the size of the screen and the view and projection matrices which you will often have stored in a uniform buffer.

```cpp
zest_vec3 zest_ScreenRay(
	float xpos, 
	float ypos, 
	float view_width, 
	float view_height, 
	zest_matrix4 *projection, 
	zest_matrix4 *view
);
```

A typical usage might be something like:

```cpp
zest_uniform_buffer uniform_buffer = zest_GetUniformBuffer(buffer_handle);
uniform_buffer_data_t *data = (uniform_buffer_data_t *)zest_GetUniformBufferData(uniform_buffer);
zest_vec3 camera_last_ray = zest_ScreenRay(x, y, zest_ScreenWidthf(context), zest_ScreenHeightf(context), &data->proj, &data->view);
```

---

### zest_WorldToScreen

```cpp
zest_vec2 zest_WorldToScreen(
	const float point[3], 
	float view_width, 
	float view_height, 
	zest_matrix4* projection, 
	zest_matrix4* view
);
```

---

### zest_WorldToScreenOrtho

```cpp
zest_vec2 zest_WorldToScreenOrtho(
	const float point[3], 
	float view_width, 
	float view_height, 
	zest_matrix4* projection, 
	zest_matrix4* view
);
```

---

## Frustum Culling

### zest_CalculateFrustumPlanes

```cpp
void zest_CalculateFrustumPlanes(
	zest_matrix4 *view_matrix, 
	zest_matrix4 *proj_matrix, 
	zest_vec4 planes[6]);
```

---

### zest_IsPointInFrustum

```cpp
zest_bool zest_IsPointInFrustum(const zest_vec4 planes[6], const float point[3]);
```

---

### zest_IsSphereInFrustum

```cpp
zest_bool zest_IsSphereInFrustum(const zest_vec4 planes[6], const float point[3], float radius);
```

---

### zest_RayIntersectPlane

```cpp
zest_bool zest_RayIntersectPlane(
	zest_vec3 ray_origin, 
	zest_vec3 ray_direction, 
	zest_vec3 plane, 
	zest_vec3 plane_normal, 
	float *distance, 
	zest_vec3 *intersection
);
```

---

## See Also

- [Math API](math.md)
