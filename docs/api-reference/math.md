# Math API

Vector, matrix, and utility math functions for 3D graphics and game development. I expect that most people will just roll with there own or use glm or something but for the purposes of examples Zest has basic vector and matrix math for various things.

## Vector Types

```cpp
zest_vec2    // { float x, y }
zest_vec3    // { float x, y, z }
zest_vec4    // { float x, y, z, w } 
zest_ivec2   // { int x, y }
zest_ivec3   // { int x, y, z }
zest_matrix4 // 4x4 matrix (array of 4 zest_vec4) 
```

---

## Vector Construction

### `zest_Vec2Set` / `zest_Vec3Set` / `zest_Vec4Set`

Create a vector with specified component values.

```cpp
zest_vec2 zest_Vec2Set(float x, float y);
zest_vec3 zest_Vec3Set(float x, float y, float z);
zest_vec4 zest_Vec4Set(float x, float y, float z, float w);
```

**Usage:** Initialize vectors for positions, directions, colors, or any multi-component data.

```cpp
zest_vec3 position = zest_Vec3Set(10.0f, 5.0f, -3.0f);
zest_vec4 color = zest_Vec4Set(1.0f, 0.5f, 0.0f, 1.0f);  // Orange, full alpha
```

### `zest_Vec2Set1` / `zest_Vec3Set1` / `zest_Vec4Set1`

Create a vector with all components set to the same value.

```cpp
zest_vec2 zest_Vec2Set1(float v);
zest_vec3 zest_Vec3Set1(float v);
zest_vec4 zest_Vec4Set1(float v);
```

**Usage:** Initialize uniform vectors for scaling, clearing, or default values.

```cpp
zest_vec3 uniform_scale = zest_Vec3Set1(2.0f);  // Scale by 2x in all directions
zest_vec4 white = zest_Vec4Set1(1.0f);          // White color with full alpha
```

---

## Vector Operations

### Addition

Add two vectors component-wise.

```cpp
zest_vec2 zest_AddVec2(zest_vec2 left, zest_vec2 right);
zest_vec3 zest_AddVec3(zest_vec3 left, zest_vec3 right);
zest_vec4 zest_AddVec4(zest_vec4 left, zest_vec4 right);
zest_ivec2 zest_AddiVec2(zest_ivec2 left, zest_ivec2 right);
zest_ivec3 zest_AddiVec3(zest_ivec3 left, zest_ivec3 right);
```

**Usage:** Move objects, combine velocities, or accumulate forces.

```cpp
zest_vec3 position = zest_Vec3Set(0, 0, 0);
zest_vec3 velocity = zest_Vec3Set(1, 0, 0);
position = zest_AddVec3(position, velocity);  // Move object
```

### Subtraction

Subtract two vectors component-wise.

```cpp
zest_vec2 zest_SubVec2(zest_vec2 left, zest_vec2 right);
zest_vec3 zest_SubVec3(zest_vec3 left, zest_vec3 right);
zest_vec4 zest_SubVec4(zest_vec4 left, zest_vec4 right);
```

**Usage:** Calculate direction vectors, distances, or relative positions.

```cpp
zest_vec3 player = zest_Vec3Set(10, 0, 5);
zest_vec3 enemy = zest_Vec3Set(20, 0, 15);
zest_vec3 to_enemy = zest_SubVec3(enemy, player);  // Direction from player to enemy
```

### Multiplication

Multiply two vectors component-wise (Hadamard product).

```cpp
zest_vec3 zest_MulVec3(zest_vec3 left, zest_vec3 right);
zest_vec4 zest_MulVec4(zest_vec4 left, zest_vec4 right);
```

**Usage:** Apply per-axis scaling, combine color with lighting, or modulate values.

```cpp
zest_vec3 base_scale = zest_Vec3Set(1, 2, 1);
zest_vec3 modifier = zest_Vec3Set(2, 1, 3);
zest_vec3 final_scale = zest_MulVec3(base_scale, modifier);  // (2, 2, 3)
```

### Division

Divide two vectors component-wise.

```cpp
zest_vec3 zest_DivVec3(zest_vec3 left, zest_vec3 right);
zest_vec4 zest_DivVec4(zest_vec4 left, zest_vec4 right);
```

**Usage:** Compute ratios or undo scaling operations.

```cpp
zest_vec3 world_size = zest_Vec3Set(100, 50, 100);
zest_vec3 grid_cells = zest_Vec3Set(10, 5, 10);
zest_vec3 cell_size = zest_DivVec3(world_size, grid_cells);  // (10, 10, 10)
```

### Scaling

Multiply a vector by a scalar value.

```cpp
zest_vec2 zest_ScaleVec2(zest_vec2 v, float s);
zest_vec3 zest_ScaleVec3(zest_vec3 v, float s);
zest_vec4 zest_ScaleVec4(zest_vec4 v, float s);
```

**Usage:** Scale velocity by delta time, adjust magnitude, or apply uniform transformations.

```cpp
zest_vec3 velocity = zest_Vec3Set(10, 5, 0);
float delta_time = 0.016f;
zest_vec3 frame_movement = zest_ScaleVec3(velocity, delta_time);
```

### Normalization

Convert a vector to unit length (magnitude of 1).

```cpp
zest_vec2 zest_NormalizeVec2(zest_vec2 v);
zest_vec3 zest_NormalizeVec3(zest_vec3 v);
zest_vec4 zest_NormalizeVec4(zest_vec4 v);
```

**Usage:** Get direction vectors, prepare vectors for dot/cross products, or ensure consistent movement speed.

```cpp
zest_vec3 movement = zest_Vec3Set(3, 0, 4);
zest_vec3 direction = zest_NormalizeVec3(movement);  // (0.6, 0, 0.8)
zest_vec3 scaled_movement = zest_ScaleVec3(direction, speed);  // Move at constant speed
```

### Length

Calculate the magnitude (length) of a vector.

```cpp
float zest_LengthVec2(zest_vec2 v);
float zest_LengthVec3(zest_vec3 v);

// Non-sqrt versions (squared length) - faster for comparisons
float zest_LengthVec2NS(zest_vec2 v);
float zest_LengthVec3NS(zest_vec3 v);
float zest_LengthVec4NS(zest_vec4 v);
```

**Usage:** Calculate distances, check if vectors exceed thresholds, or validate normalization.

```cpp
zest_vec3 velocity = zest_Vec3Set(3, 4, 0);
float speed = zest_LengthVec3(velocity);  // 5.0

// Use squared length for faster distance comparisons
float dist_sq = zest_LengthVec3NS(zest_SubVec3(a, b));
if (dist_sq < radius * radius) { /* collision */ }
```

### Flip

Negate all components of a vector.

```cpp
zest_vec3 zest_FlipVec3(zest_vec3 v);
```

**Usage:** Reverse direction, create opposite forces, or reflect vectors.

```cpp
zest_vec3 forward = zest_Vec3Set(0, 0, 1);
zest_vec3 backward = zest_FlipVec3(forward);  // (0, 0, -1)
```

### Dot Product

Calculate the dot product (scalar product) of two vectors.

```cpp
float zest_DotProduct3(const zest_vec3 a, const zest_vec3 b);
float zest_DotProduct4(const zest_vec4 a, const zest_vec4 b);
```

**Usage:** Calculate angles between vectors, project vectors, determine if vectors face the same direction, or compute lighting.

```cpp
zest_vec3 surface_normal = zest_Vec3Set(0, 1, 0);
zest_vec3 light_dir = zest_NormalizeVec3(zest_Vec3Set(1, 1, 0));
float intensity = zest_DotProduct3(surface_normal, light_dir);  // Diffuse lighting

// Check if facing same direction (positive = same, negative = opposite)
if (zest_DotProduct3(player_forward, to_enemy) > 0) { /* enemy is in front */ }
```

### Cross Product

Calculate the cross product of two 3D vectors, producing a perpendicular vector.

```cpp
zest_vec3 zest_CrossProduct(const zest_vec3 a, const zest_vec3 b);
```

**Usage:** Calculate surface normals, create perpendicular vectors, or determine winding order.

```cpp
zest_vec3 edge1 = zest_SubVec3(v1, v0);
zest_vec3 edge2 = zest_SubVec3(v2, v0);
zest_vec3 normal = zest_NormalizeVec3(zest_CrossProduct(edge1, edge2));

// Create a coordinate frame
zest_vec3 up = zest_Vec3Set(0, 1, 0);
zest_vec3 forward = zest_Vec3Set(0, 0, 1);
zest_vec3 right = zest_CrossProduct(up, forward);
```

### Distance

Calculate the 2D distance between two points.

```cpp
float zest_Distance(float fromx, float fromy, float tox, float toy);
```

**Usage:** Quick 2D distance calculations for UI, 2D games, or screen-space operations.

```cpp
float dist = zest_Distance(player_x, player_y, target_x, target_y);
if (dist < pickup_radius) { /* collect item */ }
```

---

## Matrix Operations

### Creation

#### `zest_M4`

Create a diagonal matrix (identity matrix when v=1).

```cpp
zest_matrix4 zest_M4(float v);
```

**Usage:** Initialize identity matrices or create uniform scaling matrices.

```cpp
zest_matrix4 identity = zest_M4(1.0f);   // Identity matrix
zest_matrix4 scaled = zest_M4(2.0f);     // Uniform scale by 2
```

#### `zest_CreateMatrix4`

Create a complete transformation matrix with rotation, translation, and scale in one call.

```cpp
zest_matrix4 zest_CreateMatrix4(float pitch, float yaw, float roll,
                                 float x, float y, float z,
                                 float sx, float sy, float sz);
```

- `pitch, yaw, roll` - Rotation angles in radians
- `x, y, z` - Translation (position)
- `sx, sy, sz` - Scale factors

**Usage:** Create a full object transformation matrix efficiently.

```cpp
zest_matrix4 transform = zest_CreateMatrix4(
    0.0f, zest_Radians(45.0f), 0.0f,  // Rotated 45 degrees on Y
    10.0f, 0.0f, 5.0f,                 // Position
    1.0f, 1.0f, 1.0f                   // Uniform scale
);
```

### Transformations

#### `zest_MatrixTransform`

Multiply two 4x4 matrices together.

```cpp
zest_matrix4 zest_MatrixTransform(zest_matrix4 *in, zest_matrix4 *m);
```

**Usage:** Combine multiple transformations (e.g., model-view-projection).

```cpp
zest_matrix4 model = zest_CreateMatrix4(...);
zest_matrix4 view = zest_LookAt(...);
zest_matrix4 model_view = zest_MatrixTransform(&view, &model);
zest_matrix4 mvp = zest_MatrixTransform(&projection, &model_view);
```

#### `zest_MatrixTransformVector`

Transform a vector by a matrix.

```cpp
zest_vec4 zest_MatrixTransformVector(zest_matrix4 *mat, zest_vec4 vec);
```

**Usage:** Apply transformations to points or directions.

```cpp
zest_vec4 local_pos = zest_Vec4Set(1, 0, 0, 1);  // w=1 for points
zest_vec4 world_pos = zest_MatrixTransformVector(&model_matrix, local_pos);

zest_vec4 local_dir = zest_Vec4Set(0, 1, 0, 0);  // w=0 for directions
zest_vec4 world_dir = zest_MatrixTransformVector(&model_matrix, local_dir);
```

#### `zest_ScaleMatrix4`

Scale a matrix uniformly by a scalar value.

```cpp
zest_matrix4 zest_ScaleMatrix4(zest_matrix4 *m, float scalar);
```

**Usage:** Apply uniform scaling to an existing matrix.

```cpp
zest_matrix4 base = zest_M4(1.0f);
zest_matrix4 doubled = zest_ScaleMatrix4(&base, 2.0f);
```

#### `zest_ScaleMatrix4x4`

Scale a matrix by a vec4 (per-axis scaling).

```cpp
zest_matrix4 zest_ScaleMatrix4x4(zest_matrix4 *m, zest_vec4 *v);
```

**Usage:** Apply non-uniform scaling to a matrix.

```cpp
zest_matrix4 matrix = zest_M4(1.0f);
zest_vec4 scale = zest_Vec4Set(2.0f, 1.0f, 0.5f, 1.0f);
zest_matrix4 scaled = zest_ScaleMatrix4x4(&matrix, &scale);
```

### Rotations

Create rotation matrices around each axis. Angles are in radians.

```cpp
zest_matrix4 zest_Matrix4RotateX(float angle);
zest_matrix4 zest_Matrix4RotateY(float angle);
zest_matrix4 zest_Matrix4RotateZ(float angle);
```

**Usage:** Build rotation matrices to combine with other transformations.

```cpp
float angle = zest_Radians(45.0f);
zest_matrix4 rot_y = zest_Matrix4RotateY(angle);

// Combine rotations
zest_matrix4 rot_x = zest_Matrix4RotateX(pitch);
zest_matrix4 rot_y = zest_Matrix4RotateY(yaw);
zest_matrix4 rotation = zest_MatrixTransform(&rot_y, &rot_x);
```

### Transpose

Swap rows and columns of a matrix.

```cpp
zest_matrix4 zest_TransposeMatrix4(zest_matrix4 *mat);
```

**Usage:** Convert between row-major and column-major, or compute the inverse of orthonormal matrices.

```cpp
zest_matrix4 transposed = zest_TransposeMatrix4(&matrix);
```

### Inverse

Calculate the inverse of a 4x4 matrix.

```cpp
zest_matrix4 zest_Inverse(zest_matrix4 *m);
```

**Usage:** Undo transformations, create view matrices from camera transforms, or transform from world to local space.

```cpp
zest_matrix4 model = zest_CreateMatrix4(...);
zest_matrix4 inverse_model = zest_Inverse(&model);

// Transform world point to local space
zest_vec4 local = zest_MatrixTransformVector(&inverse_model, world_point);
```

### View/Projection

#### `zest_LookAt`

Create a view matrix that looks from `eye` toward `center`.

```cpp
zest_matrix4 zest_LookAt(const zest_vec3 eye, const zest_vec3 center, const zest_vec3 up);
```

**Usage:** Create camera view matrices.

```cpp
zest_vec3 camera_pos = zest_Vec3Set(0, 5, 10);
zest_vec3 target = zest_Vec3Set(0, 0, 0);
zest_vec3 up = zest_Vec3Set(0, 1, 0);
zest_matrix4 view = zest_LookAt(camera_pos, target, up);
```

#### `zest_Perspective`

Create a perspective projection matrix.

```cpp
zest_matrix4 zest_Perspective(float fovy, float aspect, float zNear, float zFar);
```

- `fovy` - Vertical field of view in radians
- `aspect` - Aspect ratio (width / height)
- `zNear, zFar` - Near and far clipping planes

**Usage:** Standard 3D perspective rendering.

```cpp
float fov = zest_Radians(60.0f);
float aspect = (float)width / (float)height;
zest_matrix4 proj = zest_Perspective(fov, aspect, 0.1f, 1000.0f);
```

#### `zest_Ortho`

Create an orthographic projection matrix.

```cpp
zest_matrix4 zest_Ortho(float left, float right, float bottom, float top,
                         float z_near, float z_far);
```

**Usage:** 2D rendering, UI, shadow maps, or isometric views.

```cpp
// Screen-space orthographic for UI
zest_matrix4 ortho = zest_Ortho(0, screen_width, screen_height, 0, -1, 1);

// Centered orthographic
float hw = width * 0.5f;
float hh = height * 0.5f;
zest_matrix4 ortho = zest_Ortho(-hw, hw, -hh, hh, 0.1f, 100.0f);
```

---

## Interpolation

### `zest_Lerp`

Linearly interpolate between two float values.

```cpp
float zest_Lerp(float from, float to, float t);
```

### `zest_LerpVec2` / `zest_LerpVec3` / `zest_LerpVec4`

Linearly interpolate between two vectors.

```cpp
zest_vec2 zest_LerpVec2(zest_vec2 *from, zest_vec2 *to, float t);
zest_vec3 zest_LerpVec3(zest_vec3 *from, zest_vec3 *to, float t);
zest_vec4 zest_LerpVec4(zest_vec4 *from, zest_vec4 *to, float t);
```

- `t = 0` returns `from`
- `t = 1` returns `to`
- `0 < t < 1` returns a blend between them

**Usage:** Smooth animations, camera transitions, color fading, or physics interpolation.

```cpp
// Smooth camera movement
zest_vec3 current_pos = ...;
zest_vec3 target_pos = ...;
float smoothing = 0.1f;
current_pos = zest_LerpVec3(&current_pos, &target_pos, smoothing);

// Color transition
zest_vec4 start_color = zest_Vec4Set(1, 0, 0, 1);  // Red
zest_vec4 end_color = zest_Vec4Set(0, 0, 1, 1);    // Blue
zest_vec4 blended = zest_LerpVec4(&start_color, &end_color, 0.5f);  // Purple
```

---

## Angle Conversion

### `zest_Radians`

Convert degrees to radians.

```cpp
float zest_Radians(float degrees);
```

### `zest_Degrees`

Convert radians to degrees.

```cpp
float zest_Degrees(float radians);
```

**Usage:** Interface between user-friendly angles (degrees) and math functions (radians).

```cpp
float fov_degrees = 60.0f;
float fov_radians = zest_Radians(fov_degrees);

zest_matrix4 rot = zest_Matrix4RotateY(zest_Radians(90.0f));  // Rotate 90 degrees
```

---

## Packing Functions

Packing functions compress floating-point vectors into smaller integer formats for efficient GPU storage.

### `zest_Pack10bit`

Pack a normalized vec3 into a 32-bit unsigned int (10 bits per component + 2 extra bits). Note that this packing will not work on Mac GPUs (the vertex attribute description is not available for it).

```cpp
zest_uint zest_Pack10bit(zest_vec3 *v, zest_uint extra);
```

**Usage:** Compress normals or directions with 2 bits for extra data (e.g., material ID, flags).

```cpp
zest_vec3 normal = zest_Vec3Set(0.707f, 0.707f, 0.0f);
zest_uint packed = zest_Pack10bit(&normal, 0);  // extra bits = 0
```

### `zest_Pack8bitx3`

Pack a vec3 into a 32-bit unsigned int (8 bits per component).

```cpp
zest_uint zest_Pack8bitx3(zest_vec3 *v);
```

### `zest_Pack8bit`

Pack three floats into a 32-bit unsigned int (8 bits each).

```cpp
zest_uint zest_Pack8bit(float x, float y, float z);
```

**Usage:** Compress colors or low-precision vectors.

```cpp
zest_uint packed_color = zest_Pack8bit(1.0f, 0.5f, 0.0f);  // Orange
```

### `zest_Pack16bit2SNorm`

Pack two normalized floats (-1 to 1) into a 32-bit unsigned int (16 bits each).

```cpp
zest_uint zest_Pack16bit2SNorm(float x, float y);
```

### `zest_Pack16bit4SNorm`

Pack four normalized floats (-1 to 1) into a 64-bit unsigned int (16 bits each).

```cpp
zest_u64 zest_Pack16bit4SNorm(float x, float y, float z, float w);
```

**Usage:** Compress UV coordinates, tangent vectors, or other normalized data.

```cpp
zest_uint packed_uv = zest_Pack16bit2SNorm(u * 2.0f - 1.0f, v * 2.0f - 1.0f);
```

### `zest_Pack16bit2SScaled`

Pack two floats into a 32-bit unsigned int with scaling.

```cpp
zest_uint zest_Pack16bit2SScaled(float x, float y, float max_value);
```

### `zest_Pack16bit4SScaled`

Pack four floats into a 64-bit unsigned int with separate XY and ZW scaling.

```cpp
zest_u64 zest_Pack16bit4SScaled(float x, float y, float z, float w,
                                 float max_value_xy, float max_value_zw);
```

### `zest_Pack16bit4SFloat`

Pack four 32-bit floats into four 16-bit half-precision floats.

```cpp
zest_u64 zest_Pack16bit4SFloat(float x, float y, float z, float w);
```

**Usage:** GPU-compatible half-precision storage with full float range.

### `zest_FloatToHalf`

Convert a single 32-bit float to a 16-bit half-precision float.

```cpp
zest_uint zest_FloatToHalf(float f);
```

**Usage:** Manual half-float conversion for custom packing.

```cpp
zest_uint half_value = zest_FloatToHalf(3.14159f);
```

### `zest_Pack16bitStretch`

Pack two floats into a 32-bit unsigned int using stretch encoding.

```cpp
zest_uint zest_Pack16bitStretch(float x, float y);
```

---

## Color

### `zest_ColorSet`

Create a color from RGBA byte values (0-255).

```cpp
zest_color_t zest_ColorSet(zest_byte r, zest_byte g, zest_byte b, zest_byte a);
```

**Usage:** Create colors from typical 8-bit color values.

```cpp
zest_color_t red = zest_ColorSet(255, 0, 0, 255);
zest_color_t semi_transparent_blue = zest_ColorSet(0, 0, 255, 128);
```

### `zest_ColorSet1`

Create a grayscale color with all components set to the same value.

```cpp
zest_color_t zest_ColorSet1(zest_byte c);
```

**Usage:** Quick grayscale colors.

```cpp
zest_color_t white = zest_ColorSet1(255);
zest_color_t gray = zest_ColorSet1(128);
zest_color_t black = zest_ColorSet1(0);
```

---

## See Also

- [Camera API](camera.md)
