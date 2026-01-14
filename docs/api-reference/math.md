# Math API

Vector, matrix, and utility math functions (~80 functions).

## Vector Types

```cpp
zest_vec2   // { float x, y }
zest_vec3   // { float x, y, z }
zest_vec4   // { float x, y, z, w }
zest_matrix4 // 4x4 matrix
```

## Vector Construction

```cpp
zest_vec2 zest_Vec2Set(float x, float y);
zest_vec3 zest_Vec3Set(float x, float y, float z);
zest_vec4 zest_Vec4Set(float x, float y, float z, float w);

zest_vec2 zest_Vec2Set1(float v);  // All components same
zest_vec3 zest_Vec3Set1(float v);
zest_vec4 zest_Vec4Set1(float v);
```

## Vector Operations

### Addition / Subtraction

```cpp
zest_vec2 zest_AddVec2(zest_vec2 a, zest_vec2 b);
zest_vec3 zest_AddVec3(zest_vec3 a, zest_vec3 b);
zest_vec4 zest_AddVec4(zest_vec4 a, zest_vec4 b);

zest_vec2 zest_SubVec2(zest_vec2 a, zest_vec2 b);
zest_vec3 zest_SubVec3(zest_vec3 a, zest_vec3 b);
zest_vec4 zest_SubVec4(zest_vec4 a, zest_vec4 b);
```

### Multiplication / Division

```cpp
zest_vec2 zest_MulVec2(zest_vec2 a, zest_vec2 b);
zest_vec3 zest_MulVec3(zest_vec3 a, zest_vec3 b);
zest_vec4 zest_MulVec4(zest_vec4 a, zest_vec4 b);

zest_vec3 zest_DivVec3(zest_vec3 a, zest_vec3 b);
zest_vec4 zest_DivVec4(zest_vec4 a, zest_vec4 b);
```

### Scaling

```cpp
zest_vec2 zest_ScaleVec2(zest_vec2 v, float s);
zest_vec3 zest_ScaleVec3(zest_vec3 v, float s);
zest_vec4 zest_ScaleVec4(zest_vec4 v, float s);
```

### Normalization

```cpp
zest_vec2 zest_NormalizeVec2(zest_vec2 v);
zest_vec3 zest_NormalizeVec3(zest_vec3 v);
zest_vec4 zest_NormalizeVec4(zest_vec4 v);
```

### Length

```cpp
float zest_LengthVec3(zest_vec3 v);
```

### Dot Product

```cpp
float zest_DotProduct3(zest_vec3 a, zest_vec3 b);
float zest_DotProduct4(zest_vec4 a, zest_vec4 b);
```

### Cross Product

```cpp
zest_vec3 zest_CrossProduct(zest_vec3 a, zest_vec3 b);
```

## Matrix Operations

### Creation

```cpp
zest_matrix4 zest_CreateMatrix4(float diagonal);  // Identity: diagonal=1
```

### Transformations

```cpp
zest_matrix4 zest_MatrixTransform(zest_matrix4 *m, zest_vec3 translation);
zest_matrix4 zest_ScaleMatrix4(zest_matrix4 *m, zest_vec3 scale);
```

### Rotations

```cpp
zest_matrix4 zest_Matrix4RotateX(zest_matrix4 *m, float angle);
zest_matrix4 zest_Matrix4RotateY(zest_matrix4 *m, float angle);
zest_matrix4 zest_Matrix4RotateZ(zest_matrix4 *m, float angle);
```

### View/Projection

```cpp
zest_matrix4 zest_LookAt(zest_vec3 eye, zest_vec3 center, zest_vec3 up);
zest_matrix4 zest_Perspective(float fov, float aspect, float near, float far);
zest_matrix4 zest_Ortho(float left, float right, float bottom, float top, float near, float far);
```

### Inverse

```cpp
zest_matrix4 zest_Inverse(zest_matrix4 *m);
```

## Interpolation

```cpp
float zest_Lerp(float a, float b, float t);
zest_vec2 zest_LerpVec2(zest_vec2 a, zest_vec2 b, float t);
zest_vec3 zest_LerpVec3(zest_vec3 a, zest_vec3 b, float t);
zest_vec4 zest_LerpVec4(zest_vec4 a, zest_vec4 b, float t);
```

## Angle Conversion

```cpp
float zest_Radians(float degrees);
float zest_Degrees(float radians);
```

## Packing Functions

```cpp
zest_uint zest_Pack10bit(zest_vec3 v);
zest_uint zest_Pack8bit(zest_vec4 v);
zest_uint zest_Pack16bit2SNorm(zest_vec2 v);
zest_half zest_FloatToHalf(float f);
```

## Color

```cpp
zest_color_t zest_ColorSet(zest_byte r, zest_byte g, zest_byte b, zest_byte a);
zest_color_t zest_ColorSet1(zest_byte v);
```

---

## See Also

- [Camera API](camera.md)
