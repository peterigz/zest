#ifndef ZEST_IMPL_H
#define ZEST_IMPL_H

zest__platform_setup zest__platform_setup_callbacks[zest_max_platforms] = { 0 };
//The thread local frame graph setup context
static ZEST_THREAD_LOCAL zest_frame_graph_builder zest__frame_graph_builder = NULL;

// --[Struct_definitions]
typedef struct zest_mesh_t {
    int magic;
	zest_context context;
    zest_vertex_t* vertices;
    zest_uint* indexes;
} zest_mesh_t;

typedef struct zest_output_group_t {
    int magic;
    zest_resource_node *resources;
} zest_output_group_t;

typedef struct zest_timer_t {
    int magic;
    zest_timer_handle handle;
    double start_time;
    double delta_time;
    double update_frequency;
    double update_tick_length;
    double update_time;
    double ticker;
    double accumulator;
    double accumulator_delta;
    double current_time;
    double lerp;
    double time_passed;
    double seconds_passed;
    double max_elapsed_time;
    int update_count;
} zest_timer_t;

typedef struct zest_buffer_allocator_t {
    int magic;
	zest_context context;
    zest_buffer_info_t buffer_info;
    zloc_allocator *allocator;
    zest_size alignment;
    zest_device_memory_pool *memory_pools;
    zest_pool_range *range_pools;
} zest_buffer_allocator_t;

// -- End Struct Definions

#ifdef _WIN32
zest_millisecs zest_Millisecs(void) {
    LARGE_INTEGER frequency, counter;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    zest_ull ms = (zest_ull)(counter.QuadPart * 1000LL / frequency.QuadPart);
    return (zest_millisecs)ms;
}

zest_microsecs zest_Microsecs(void) {
    LARGE_INTEGER frequency, counter;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    zest_ull us = (zest_ull)(counter.QuadPart * 1000000LL / frequency.QuadPart);
    return (zest_microsecs)us;
}
#elif defined(__APPLE__)
#include <mach/mach_time.h>
zest_millisecs zest_Millisecs(void) {
    static mach_timebase_info_data_t timebase_info;
    if (timebase_info.denom == 0) {
        mach_timebase_info(&timebase_info);
    }

    uint64_t time_ns = mach_absolute_time() * timebase_info.numer / timebase_info.denom;
    zest_millisecs ms = (zest_millisecs)(time_ns / 1000000);
    return (zest_millisecs)ms;
}

zest_microsecs zest_Microsecs(void) {
    static mach_timebase_info_data_t timebase_info;
    if (timebase_info.denom == 0) {
        mach_timebase_info(&timebase_info);
    }

    uint64_t time_ns = mach_absolute_time() * timebase_info.numer / timebase_info.denom;
    zest_microsecs us = (zest_microsecs)(time_ns / 1000);
    return us;
}
#else
zest_millisecs zest_Millisecs(void) {
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    long m = now.tv_sec * 1000 + now.tv_nsec / 1000000;
    return (zest_millisecs)m;
}

zest_microsecs zest_Microsecs(void) {
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    zest_ull us = now.tv_sec * 1000000ULL + now.tv_nsec / 1000;
    return (zest_microsecs)us;
}
#endif

#ifdef _WIN32
FILE* zest__open_file(const char* file_name, const char* mode) {
    FILE* file = NULL;
    errno_t err = fopen_s(&file, file_name, mode);
    if (err != 0 || file == NULL) {
        return NULL;
    }
    return file;
}

#else

FILE* zest__open_file(const char* file_name, const char* mode) {
    return fopen(file_name, mode);
}

#endif

zest_bool zest__file_exists(const char* file_name) {
	FILE* file = zest__open_file(file_name, "r");
	if (file) {
		fclose(file);
		return ZEST_TRUE;
	}
	return ZEST_FALSE;
}

bool zest__create_folder(zest_context context, const char *path) {
    int result = ZEST_CREATE_DIR(path);
    if (result == 0) {
        ZEST_APPEND_LOG(context->device->log_path.str, "Folder created successfully: %s", path);
        return true;
    } else {
        if (result == -1) {
            return true;
        } else {
            char buffer[100];
            if (zest_strerror(buffer, sizeof(buffer), result) != 0) {
				ZEST_APPEND_LOG(context->device->log_path.str, "Error creating folder: %s (Error: Unknown)", path);
            } else {
				ZEST_APPEND_LOG(context->device->log_path.str, "Error creating folder: %s (Error: %s)", path, buffer);
            }
            return false;
        }
    }
}

// --Math
zest_matrix4 zest_M4(float v) {
    zest_matrix4 matrix = ZEST__ZERO_INIT(zest_matrix4);
    matrix.v[0].x = v;
    matrix.v[1].y = v;
    matrix.v[2].z = v;
    matrix.v[3].w = v;
    return matrix;
}

zest_matrix4 zest_M4SetWithVecs(zest_vec4 a, zest_vec4 b, zest_vec4 c, zest_vec4 d) {
    zest_matrix4 r;
    r.v[0].x = a.x; r.v[0].y = a.y; r.v[0].z = a.z; r.v[0].w = a.w;
    r.v[1].x = b.x; r.v[1].y = b.y; r.v[1].z = b.z; r.v[1].w = b.w;
    r.v[2].x = c.x; r.v[2].y = c.y; r.v[2].z = c.z; r.v[2].w = c.w;
    r.v[3].x = d.x; r.v[3].y = d.y; r.v[3].z = d.z; r.v[3].w = d.w;
    return r;
}

zest_matrix4 zest_ScaleMatrix4x4(zest_matrix4* m, zest_vec4* v) {
    zest_matrix4 result = zest_M4(1);
    result.v[0] = zest_ScaleVec4(m->v[0], v->x);
    result.v[1] = zest_ScaleVec4(m->v[1], v->y);
    result.v[2] = zest_ScaleVec4(m->v[2], v->z);
    result.v[3] = zest_ScaleVec4(m->v[3], v->w);
    return result;
}

zest_matrix4 zest_ScaleMatrix4(zest_matrix4* m, float scalar) {
    zest_matrix4 result = zest_M4(1);
    result.v[0] = zest_ScaleVec4(m->v[0], scalar);
    result.v[1] = zest_ScaleVec4(m->v[1], scalar);
    result.v[2] = zest_ScaleVec4(m->v[2], scalar);
    result.v[3] = zest_ScaleVec4(m->v[3], scalar);
    return result;
}

zest_vec2 zest_Vec2Set1(float v) {
    zest_vec2 vec; vec.x = v; vec.y = v; return vec;
}

zest_vec3 zest_Vec3Set1(float v) {
    zest_vec3 vec; vec.x = v; vec.y = v; vec.z = v; return vec;
}

zest_vec4 zest_Vec4Set1(float v) {
    zest_vec4 vec; vec.x = v; vec.y = v; vec.z = v; vec.w = v; return vec;
}

zest_vec2 zest_Vec2Set(float x, float y) {
    zest_vec2 vec; vec.x = x; vec.y = y; return vec;
}

zest_vec3 zest_Vec3Set(float x, float y, float z) {
    zest_vec3 vec; vec.x = x; vec.y = y; vec.z = z; return vec;
}

zest_vec4 zest_Vec4Set(float x, float y, float z, float w) {
    zest_vec4 vec; vec.x = x; vec.y = y; vec.z = z; vec.w = w; return vec;
}

zest_vec4 zest_Vec4SetVec(zest_vec4 in) {
    zest_vec4 vec; vec.x = in.x; vec.y = in.y; vec.z = in.z; vec.w = in.w; return vec;
}

zest_color_t zest_ColorSet(zest_byte r, zest_byte g, zest_byte b, zest_byte a) {
	zest_color_t color;
	color.r = r;
	color.g = g;
	color.b = b;
	color.a = a;
    return color;
}

zest_color_t zest_ColorSet1(zest_byte c) {
	zest_color_t color;
	color.r = c;
	color.g = c;
	color.b = c;
	color.a = c;
    return color;
}

zest_vec2 zest_AddVec2(zest_vec2 left, zest_vec2 right) {
	zest_vec2 result;
	result.x = left.x + right.x;
	result.y = left.y + right.y;
    return result;
}

zest_vec3 zest_AddVec3(zest_vec3 left, zest_vec3 right) {
	zest_vec3 result;
	result.x = left.x + right.x;
	result.y = left.y + right.y;
	result.z = left.z + right.z;
    return result;
}

zest_vec4 zest_AddVec4(zest_vec4 left, zest_vec4 right) {
	zest_vec4 result;
	result.x = left.x + right.x;
	result.y = left.y + right.y;
	result.z = left.z + right.z;
	result.w = left.w + right.w;
    return result;
}

zest_ivec2 zest_AddiVec2(zest_ivec2 left, zest_ivec2 right) {
	zest_ivec2 result;
	result.x = left.x + right.x;
	result.y = left.y + right.y;
    return result;
}

zest_ivec3 zest_AddiVec3(zest_ivec3 left, zest_ivec3 right) {
	zest_ivec3 result;
	result.x = left.x + right.x;
	result.y = left.y + right.y;
	result.z = left.z + right.z;
    return result;
}

zest_vec2 zest_SubVec2(zest_vec2 left, zest_vec2 right) {
	zest_vec2 result;
	result.x = left.x - right.x;
	result.y = left.y - right.y;
    return result;
}

zest_vec3 zest_SubVec3(zest_vec3 left, zest_vec3 right) {
	zest_vec3 result;
	result.x = left.x - right.x;
	result.y = left.y - right.y;
	result.z = left.z - right.z;
    return result;
}

zest_vec4 zest_SubVec4(zest_vec4 left, zest_vec4 right) {
	zest_vec4 result;
        result.x = left.x - right.x;
        result.y = left.y - right.y;
        result.z = left.z - right.z;
        result.w = left.w - right.w;
    return result;
}

zest_vec3 zest_FlipVec3(zest_vec3 vec3) {
    zest_vec3 flipped;
    flipped.x = -vec3.x;
    flipped.y = -vec3.y;
    flipped.z = -vec3.z;
    return flipped;
}

zest_vec2 zest_ScaleVec2(zest_vec2 vec, float scalar) {
    zest_vec2 result;
    result.x = vec.x * scalar;
    result.y = vec.y * scalar;
    return result;
}

zest_vec3 zest_ScaleVec3(zest_vec3 vec, float scalar) {
    zest_vec3 result;
    result.x = vec.x * scalar;
    result.y = vec.y * scalar;
    result.z = vec.z * scalar;
    return result;
}

zest_vec4 zest_ScaleVec4(zest_vec4 vec, float scalar) {
    zest_vec4 result;
    result.x = vec.x * scalar;
    result.y = vec.y * scalar;
    result.z = vec.z * scalar;
    result.w = vec.w * scalar;
    return result;
}

zest_vec3 zest_MulVec3(zest_vec3 left, zest_vec3 right) {
	zest_vec3 result;
	result.x = left.x * right.x;
	result.y = left.y * right.y;
	result.z = left.z * right.z;
    return result;
}

zest_vec4 zest_MulVec4(zest_vec4 left, zest_vec4 right) {
	zest_vec4 result;
	result.x = left.x * right.x;
	result.y = left.y * right.y;
	result.z = left.z * right.z;
	result.w = left.w * right.w;
    return result;
}

float zest_Vec2Length(zest_vec2 const v) {
    return sqrtf(zest_Vec2Length2(v));
}

float zest_Vec2Length2(zest_vec2 const v) {
    return v.x * v.x + v.y * v.y;
}

float zest_LengthVec3(zest_vec3 const v) {
    return v.x * v.x + v.y * v.y + v.z * v.z;
}

float zest_LengthVec4(zest_vec4 const v) {
    return v.x * v.x + v.y * v.y + v.z * v.z + v.w * v.w;
}

float zest_LengthVec(zest_vec3 const v) {
    return sqrtf(zest_LengthVec3(v));
}

zest_vec2 zest_NormalizeVec2(zest_vec2 const v) {
    float length = zest_Vec2Length(v);
	zest_vec2 result;
	result.x = v.x / length;
	result.y = v.y / length;
    return result;
}

zest_vec3 zest_NormalizeVec3(zest_vec3 const v) {
    float length = zest_LengthVec(v);
	zest_vec3 result;
	result.x = v.x / length;
	result.y = v.y / length;
	result.z = v.z / length;
    return result;
}

zest_vec4 zest_NormalizeVec4(zest_vec4 const v) {
    float length = sqrtf(zest_LengthVec4(v));
	zest_vec4 result;
	result.x = v.x / length;
	result.y = v.y / length;
	result.z = v.z / length;
	result.w = v.w / length;
    return result;
}

zest_matrix4 zest_Inverse(zest_matrix4* m) {
    float Coef00 = m->v[2].z * m->v[3].w - m->v[3].z * m->v[2].w;
    float Coef02 = m->v[1].z * m->v[3].w - m->v[3].z * m->v[1].w;
    float Coef03 = m->v[1].z * m->v[2].w - m->v[2].z * m->v[1].w;

    float Coef04 = m->v[2].y * m->v[3].w - m->v[3].y * m->v[2].w;
    float Coef06 = m->v[1].y * m->v[3].w - m->v[3].y * m->v[1].w;
    float Coef07 = m->v[1].y * m->v[2].w - m->v[2].y * m->v[1].w;

    float Coef08 = m->v[2].y * m->v[3].z - m->v[3].y * m->v[2].z;
    float Coef10 = m->v[1].y * m->v[3].z - m->v[3].y * m->v[1].z;
    float Coef11 = m->v[1].y * m->v[2].z - m->v[2].y * m->v[1].z;

    float Coef12 = m->v[2].x * m->v[3].w - m->v[3].x * m->v[2].w;
    float Coef14 = m->v[1].x * m->v[3].w - m->v[3].x * m->v[1].w;
    float Coef15 = m->v[1].x * m->v[2].w - m->v[2].x * m->v[1].w;

    float Coef16 = m->v[2].x * m->v[3].z - m->v[3].x * m->v[2].z;
    float Coef18 = m->v[1].x * m->v[3].z - m->v[3].x * m->v[1].z;
    float Coef19 = m->v[1].x * m->v[2].z - m->v[2].x * m->v[1].z;

    float Coef20 = m->v[2].x * m->v[3].y - m->v[3].x * m->v[2].y;
    float Coef22 = m->v[1].x * m->v[3].y - m->v[3].x * m->v[1].y;
    float Coef23 = m->v[1].x * m->v[2].y - m->v[2].x * m->v[1].y;

    zest_vec4 Fac0 = zest_Vec4Set(Coef00, Coef00, Coef02, Coef03);
    zest_vec4 Fac1 = zest_Vec4Set(Coef04, Coef04, Coef06, Coef07);
    zest_vec4 Fac2 = zest_Vec4Set(Coef08, Coef08, Coef10, Coef11);
    zest_vec4 Fac3 = zest_Vec4Set(Coef12, Coef12, Coef14, Coef15);
    zest_vec4 Fac4 = zest_Vec4Set(Coef16, Coef16, Coef18, Coef19);
    zest_vec4 Fac5 = zest_Vec4Set(Coef20, Coef20, Coef22, Coef23);

    zest_vec4 Vec0 = zest_Vec4Set(m->v[1].x, m->v[0].x, m->v[0].x, m->v[0].x);
    zest_vec4 Vec1 = zest_Vec4Set(m->v[1].y, m->v[0].y, m->v[0].y, m->v[0].y);
    zest_vec4 Vec2 = zest_Vec4Set(m->v[1].z, m->v[0].z, m->v[0].z, m->v[0].z);
    zest_vec4 Vec3 = zest_Vec4Set(m->v[1].w, m->v[0].w, m->v[0].w, m->v[0].w);

    zest_vec4 Inv0 = zest_Vec4SetVec(zest_AddVec4(zest_SubVec4(zest_MulVec4(Vec1, Fac0), zest_MulVec4(Vec2, Fac1)), zest_MulVec4(Vec3, Fac2)));
    zest_vec4 Inv1 = zest_Vec4SetVec(zest_AddVec4(zest_SubVec4(zest_MulVec4(Vec0, Fac0), zest_MulVec4(Vec2, Fac3)), zest_MulVec4(Vec3, Fac4)));
    zest_vec4 Inv2 = zest_Vec4SetVec(zest_AddVec4(zest_SubVec4(zest_MulVec4(Vec0, Fac1), zest_MulVec4(Vec1, Fac3)), zest_MulVec4(Vec3, Fac5)));
    zest_vec4 Inv3 = zest_Vec4SetVec(zest_AddVec4(zest_SubVec4(zest_MulVec4(Vec0, Fac2), zest_MulVec4(Vec1, Fac4)), zest_MulVec4(Vec2, Fac5)));

    zest_vec4 SignA = zest_Vec4Set(+1, -1, +1, -1);
    zest_vec4 SignB = zest_Vec4Set(-1, +1, -1, +1);
    zest_matrix4 inverse = zest_M4SetWithVecs(zest_MulVec4(Inv0, SignA), zest_MulVec4(Inv1, SignB), zest_MulVec4(Inv2, SignA), zest_MulVec4(Inv3, SignB));

    zest_vec4 Row0 = zest_Vec4Set(inverse.v[0].x, inverse.v[1].x, inverse.v[2].x, inverse.v[3].x);

    zest_vec4 Dot0 = zest_Vec4SetVec(zest_MulVec4(m->v[0], Row0));
    float Dot1 = (Dot0.x + Dot0.y) + (Dot0.z + Dot0.w);

    float OneOverDeterminant = 1.f / Dot1;

    return zest_ScaleMatrix4(&inverse, OneOverDeterminant);
}

zest_matrix4 zest_Matrix4RotateX(float angle) {
    float c = cosf(angle);
    float s = sinf(angle);
    zest_matrix4 r =
    { {
        {1, 0, 0, 0},
        {0, c,-s, 0},
        {0, s, c, 0},
        {0, 0, 0, 1}},
    };
    return r;
}

zest_matrix4 zest_Matrix4RotateY(float angle) {
    float c = cosf(angle);
    float s = sinf(angle);
    zest_matrix4 r =
    { {
        { c, 0, s, 0},
        { 0, 1, 0, 0},
        {-s, 0, c, 0},
        { 0, 0, 0, 1}},
    };
    return r;
}

zest_matrix4 zest_Matrix4RotateZ(float angle) {
    float c = cosf(angle);
    float s = sinf(angle);
    zest_matrix4 r =
    { {
        {c, -s, 0, 0},
        {s,  c, 0, 0},
        {0,  0, 1, 0},
        {0,  0, 0, 1}},
    };
    return r;
}

zest_matrix4 zest_CreateMatrix4(float pitch, float yaw, float roll, float x, float y, float z, float sx, float sy, float sz) {
    zest_matrix4 roll_mat = zest_Matrix4RotateZ(roll);
    zest_matrix4 pitch_mat = zest_Matrix4RotateX(pitch);
    zest_matrix4 yaw_mat = zest_Matrix4RotateY(yaw);
    zest_matrix4 matrix = zest_MatrixTransform(&yaw_mat, &pitch_mat);
    matrix = zest_MatrixTransform(&matrix, &roll_mat);
    matrix.v[0].w = x;
    matrix.v[1].w = y;
    matrix.v[2].w = z;
    matrix.v[3].x = sx;
    matrix.v[3].y = sy;
    matrix.v[3].z = sz;
    return matrix;
}

#ifdef ZEST_INTEL

zest_vec4 zest_MatrixTransformVector(zest_matrix4* mat, zest_vec4 vec) {
    zest_vec4 v;

    __m128 v4 = _mm_set_ps(vec.w, vec.z, vec.y, vec.x);

    __m128 mrow1 = _mm_load_ps(&mat->v[0].x);
    __m128 mrow2 = _mm_load_ps(&mat->v[1].x);
    __m128 mrow3 = _mm_load_ps(&mat->v[2].x);
    __m128 mrow4 = _mm_load_ps(&mat->v[3].x);

    __m128 row1result = _mm_mul_ps(v4, mrow1);
    __m128 row2result = _mm_mul_ps(v4, mrow2);
    __m128 row3result = _mm_mul_ps(v4, mrow3);
    __m128 row4result = _mm_mul_ps(v4, mrow4);

    float tmp[4];
    _mm_store_ps(tmp, row1result);
    v.x = tmp[0] + tmp[1] + tmp[2] + tmp[3];
    _mm_store_ps(tmp, row2result);
    v.y = tmp[0] + tmp[1] + tmp[2] + tmp[3];
    _mm_store_ps(tmp, row3result);
    v.z = tmp[0] + tmp[1] + tmp[2] + tmp[3];
    _mm_store_ps(tmp, row4result);
    v.w = tmp[0] + tmp[1] + tmp[2] + tmp[3];

    return v;
}

zest_matrix4 zest_MatrixTransform(zest_matrix4* in, zest_matrix4* m) {
    zest_matrix4 res = ZEST__ZERO_INIT(zest_matrix4);

    __m128 in_row[4];
    in_row[0] = _mm_load_ps(&in->v[0].x);
    in_row[1] = _mm_load_ps(&in->v[1].x);
    in_row[2] = _mm_load_ps(&in->v[2].x);
    in_row[3] = _mm_load_ps(&in->v[3].x);

    __m128 m_row1 = _mm_set_ps(m->v[3].x, m->v[2].x, m->v[1].x, m->v[0].x);
    __m128 m_row2 = _mm_set_ps(m->v[3].y, m->v[2].y, m->v[1].y, m->v[0].y);
    __m128 m_row3 = _mm_set_ps(m->v[3].z, m->v[2].z, m->v[1].z, m->v[0].z);
    __m128 m_row4 = _mm_set_ps(m->v[3].w, m->v[2].w, m->v[1].w, m->v[0].w);

    for (int r = 0; r <= 3; ++r)
    {

        __m128 row1result = _mm_mul_ps(in_row[r], m_row1);
        __m128 row2result = _mm_mul_ps(in_row[r], m_row2);
        __m128 row3result = _mm_mul_ps(in_row[r], m_row3);
        __m128 row4result = _mm_mul_ps(in_row[r], m_row4);

        float tmp[4];
        _mm_store_ps(tmp, row1result);
        res.v[r].x = tmp[0] + tmp[1] + tmp[2] + tmp[3];
        _mm_store_ps(tmp, row2result);
        res.v[r].y = tmp[0] + tmp[1] + tmp[2] + tmp[3];
        _mm_store_ps(tmp, row3result);
        res.v[r].z = tmp[0] + tmp[1] + tmp[2] + tmp[3];
        _mm_store_ps(tmp, row4result);
        res.v[r].w = tmp[0] + tmp[1] + tmp[2] + tmp[3];

    }
    return res;
}

#elif defined(ZEST_ARM)
zest_vec4 zest_MatrixTransformVector(zest_matrix4* mat, zest_vec4 vec) {
    zest_vec4 v;

    float32x4_t v4 = vld1q_f32(&vec.x);

    float32x4_t mrow1 = vld1q_f32(&mat->v[0].x);
    float32x4_t mrow2 = vld1q_f32(&mat->v[1].x);
    float32x4_t mrow3 = vld1q_f32(&mat->v[2].x);
    float32x4_t mrow4 = vld1q_f32(&mat->v[3].x);

    float32x4_t row1result = vmulq_f32(v4, mrow1);
    float32x4_t row2result = vmulq_f32(v4, mrow2);
    float32x4_t row3result = vmulq_f32(v4, mrow3);
    float32x4_t row4result = vmulq_f32(v4, mrow4);

    float tmp[4];
    vst1q_f32(tmp, row1result);
    v.x = tmp[0] + tmp[1] + tmp[2] + tmp[3];
    vst1q_f32(tmp, row2result);
    v.y = tmp[0] + tmp[1] + tmp[2] + tmp[3];
    vst1q_f32(tmp, row3result);
    v.z = tmp[0] + tmp[1] + tmp[2] + tmp[3];
    vst1q_f32(tmp, row4result);
    v.w = tmp[0] + tmp[1] + tmp[2] + tmp[3];

    return v;
}

zest_matrix4 zest_MatrixTransform(zest_matrix4* in, zest_matrix4* m) {
    zest_matrix4 res = ZEST__ZERO_INIT(zest_matrix4);

    float32x4_t in_row[4];
    in_row[0] = vld1q_f32(&in->v[0].x);
    in_row[1] = vld1q_f32(&in->v[1].x);
    in_row[2] = vld1q_f32(&in->v[2].x);
    in_row[3] = vld1q_f32(&in->v[3].x);

    float32x4_t m_row1 = vsetq_lane_f32(m->v[0].x, vdupq_n_f32(m->v[1].x), 0);
    m_row1 = vsetq_lane_f32(m->v[2].x, m_row1, 1);
    m_row1 = vsetq_lane_f32(m->v[3].x, m_row1, 2);

    float32x4_t m_row2 = vsetq_lane_f32(m->v[0].y, vdupq_n_f32(m->v[1].y), 0);
    m_row2 = vsetq_lane_f32(m->v[2].y, m_row2, 1);
    m_row2 = vsetq_lane_f32(m->v[3].y, m_row2, 2);

    float32x4_t m_row3 = vsetq_lane_f32(m->v[0].z, vdupq_n_f32(m->v[1].z), 0);
    m_row3 = vsetq_lane_f32(m->v[2].z, m_row3, 1);
    m_row3 = vsetq_lane_f32(m->v[3].z, m_row3, 2);

    float32x4_t m_row4 = vsetq_lane_f32(m->v[0].w, vdupq_n_f32(m->v[1].w), 0);
    m_row4 = vsetq_lane_f32(m->v[2].w, m_row4, 1);
    m_row4 = vsetq_lane_f32(m->v[3].w, m_row4, 2);

    for (int r = 0; r <= 3; ++r)
    {
        float32x4_t row1result = vmulq_f32(in_row[r], m_row1);
        float32x4_t row2result = vmulq_f32(in_row[r], m_row2);
        float32x4_t row3result = vmulq_f32(in_row[r], m_row3);
        float32x4_t row4result = vmulq_f32(in_row[r], m_row4);

        float32x4_t tmp;
        tmp = vaddq_f32(row1result, row2result);
        tmp = vaddq_f32(tmp, row3result);
        tmp = vaddq_f32(tmp, row4result);

        res.v[r].x = vgetq_lane_f32(tmp, 0);
        res.v[r].y = vgetq_lane_f32(tmp, 1);
        res.v[r].z = vgetq_lane_f32(tmp, 2);
        res.v[r].w = vgetq_lane_f32(tmp, 3);
    }
    return res;
}
#endif

zest_vec3 zest_CrossProduct(const zest_vec3 x, const zest_vec3 y)
{
	zest_vec3 result;
	result.x = x.y * y.z - y.y * x.z;
	result.y = x.z * y.x - y.z * x.x;
	result.z = x.x * y.y - y.x * x.y;
    return result;
}

float zest_DotProduct3(const zest_vec3 a, const zest_vec3 b)
{
    return (a.x * b.x + a.y * b.y + a.z * b.z);
}

float zest_DotProduct4(const zest_vec4 a, const zest_vec4 b)
{
    return (a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w);
}

zest_matrix4 zest_LookAt(const zest_vec3 eye, const zest_vec3 center, const zest_vec3 up)
{
    zest_vec3 f = zest_NormalizeVec3(zest_SubVec3(center, eye));
    zest_vec3 s = zest_NormalizeVec3(zest_CrossProduct(f, up));
    zest_vec3 u = zest_CrossProduct(s, f);

    zest_matrix4 result = ZEST__ZERO_INIT(zest_matrix4);
    result.v[0].x = s.x;
    result.v[1].x = s.y;
    result.v[2].x = s.z;
    result.v[0].y = u.x;
    result.v[1].y = u.y;
    result.v[2].y = u.z;
    result.v[0].z = -f.x;
    result.v[1].z = -f.y;
    result.v[2].z = -f.z;
    result.v[3].x = -zest_DotProduct3(s, eye);
    result.v[3].y = -zest_DotProduct3(u, eye);
    result.v[3].z = zest_DotProduct3(f, eye);
    result.v[3].w = 1.f;
    return result;
}

zest_matrix4 zest_Ortho(float left, float right, float bottom, float top, float z_near, float z_far)
{
    zest_matrix4 result = ZEST__ZERO_INIT(zest_matrix4);
    result.v[0].x = 2.f / (right - left);
    result.v[1].y = 2.f / (top - bottom);
    result.v[2].z = -1.f / (z_far - z_near);
    result.v[3].x = -(right + left) / (right - left);
    result.v[3].y = -(top + bottom) / (top - bottom);
    result.v[3].z = -z_near / (z_far - z_near);
    result.v[3].w = 1.f;
    return result;
}

//-- Events and States
zest_bool zest_SwapchainWasRecreated(zest_context context) {
    return ZEST__FLAGGED(context->swapchain->flags, zest_swapchain_flag_was_recreated);
}
//-- End Events and States

//-- Camera and other helpers
zest_camera_t zest_CreateCamera() {
    zest_camera_t camera = ZEST__ZERO_INIT(zest_camera_t);
    camera.yaw = 0.f;
    camera.pitch = 0.f;
    camera.fov = 1.5708f;
    camera.ortho_scale = 5.f;
    camera.up = zest_Vec3Set(0.f, 1.f, 0.f);
    camera.right = zest_Vec3Set(1.f, 0.f, 0.f);
    zest_CameraUpdateFront(&camera);
    return camera;
}

void zest_TurnCamera(zest_camera_t* camera, float turn_x, float turn_y, float sensitivity) {
    camera->yaw -= zest_Radians(turn_x * sensitivity);
    camera->pitch += zest_Radians(turn_y * sensitivity);
    camera->pitch = ZEST__CLAMP(camera->pitch, -1.55334f, 1.55334f);
    zest_CameraUpdateFront(camera);
}

void zest_CameraUpdateFront(zest_camera_t* camera) {
    zest_vec3 direction;
    direction.x = cosf(camera->yaw) * cosf(camera->pitch);
    direction.y = sinf(camera->pitch);
    direction.z = sinf(camera->yaw) * cosf(camera->pitch);
    camera->front = zest_NormalizeVec3(direction);
}

void zest_CameraMoveForward(zest_camera_t* camera, float speed) {
    camera->position = zest_AddVec3(camera->position, zest_ScaleVec3(camera->front, speed));
}

void zest_CameraMoveBackward(zest_camera_t* camera, float speed) {
    camera->position = zest_SubVec3(camera->position, zest_ScaleVec3(camera->front, speed));
}

void zest_CameraMoveUp(zest_camera_t* camera, float speed) {
    zest_vec3 cross = zest_NormalizeVec3(zest_CrossProduct(camera->front, camera->right));
    camera->position = zest_AddVec3(camera->position, zest_ScaleVec3(cross, speed));
}

void zest_CameraMoveDown(zest_camera_t* camera, float speed) {
    zest_vec3 cross = zest_NormalizeVec3(zest_CrossProduct(camera->front, camera->right));
    camera->position = zest_SubVec3(camera->position, zest_ScaleVec3(cross, speed));
}

void zest_CameraStrafLeft(zest_camera_t* camera, float speed) {
    zest_vec3 cross = zest_NormalizeVec3(zest_CrossProduct(camera->front, camera->up));
    camera->position = zest_SubVec3(camera->position, zest_ScaleVec3(cross, speed));
}

void zest_CameraStrafRight(zest_camera_t* camera, float speed) {
    zest_vec3 cross = zest_NormalizeVec3(zest_CrossProduct(camera->front, camera->up));
    camera->position = zest_AddVec3(camera->position, zest_ScaleVec3(cross, speed));
}

void zest_CameraPosition(zest_camera_t* camera, zest_vec3 position) {
    camera->position = position;
}

void zest_CameraSetFoV(zest_camera_t* camera, float degrees) {
    camera->fov = zest_Radians(degrees);
}

void zest_CameraSetPitch(zest_camera_t* camera, float degrees) {
    camera->pitch = zest_Radians(degrees);
}

void zest_CameraSetYaw(zest_camera_t* camera, float degrees) {
    camera->yaw = zest_Radians(degrees);
}

zest_bool zest_RayIntersectPlane(zest_vec3 ray_origin, zest_vec3 ray_direction, zest_vec3 plane, zest_vec3 plane_normal, float* distance, zest_vec3* intersection) {
    float ray_to_plane_normal_dp = zest_DotProduct3(plane_normal, ray_direction);
    if (ray_to_plane_normal_dp >= 0)
        return ZEST_FALSE;
    float d = zest_DotProduct3(plane, plane_normal);
    *distance = (d - zest_DotProduct3(ray_origin, plane_normal)) / ray_to_plane_normal_dp;
    *intersection = zest_ScaleVec3(ray_direction, *distance);
    *intersection = zest_AddVec3(*intersection, ray_origin);
    return ZEST_TRUE;
}

zest_vec3 zest_ScreenRay(float xpos, float ypos, float view_width, float view_height, zest_matrix4* projection, zest_matrix4* view) {
    // converts a position from the 2d xpos, ypos to a normalized 3d direction
    float x = (2.0f * xpos) / view_width - 1.f;
    float y = (2.0f * ypos) / view_height - 1.f;
    zest_vec4 ray_clip = zest_Vec4Set(x, y, -1.f, 1.0f);
    // eye space to clip we would multiply by projection so
    // clip space to eye space is the inverse projection
    zest_matrix4 inverse_projection = zest_Inverse(projection);
    zest_vec4 ray_eye = zest_MatrixTransformVector(&inverse_projection, ray_clip);
    // convert point to forwards
    ray_eye = zest_Vec4Set(ray_eye.x, ray_eye.y, -1.f, 1.0f);
    // world space to eye space is usually multiply by view so
    // eye space to world space is inverse view
    zest_vec4 inv_ray_wor = zest_MatrixTransformVector(view, ray_eye);
    zest_vec3 ray_wor = zest_Vec3Set(inv_ray_wor.x, inv_ray_wor.y, inv_ray_wor.z);
    return ray_wor;
}

zest_vec2 zest_WorldToScreen(const float point[3], float view_width, float view_height, zest_matrix4* projection, zest_matrix4* view) {
    float w = view->v[0].z * point[0] + view->v[1].z * point[1] + view->v[2].z * point[2] + view->v[3].z;
    // If you try to convert the camera's "from" position to screen space, you will
    // end up dividing by zero (please don't do that)
    //if (w <= 0) return [-1, -1];
    if (w == 0) return zest_Vec2Set(-1.f, -1.f);
    float cx = projection->v[2].x + projection->v[0].x * (view->v[0].x * point[0] + view->v[1].x * point[1] + view->v[2].x * point[2] + view->v[3].x) / w;
    float cy = projection->v[2].y + projection->v[1].y * (view->v[0].y * point[0] + view->v[1].y * point[1] + view->v[2].y * point[2] + view->v[3].y) / w;

    return zest_Vec2Set((0.5f + 0.5f * -cx) * view_width, (0.5f + 0.5f * -cy) * view_height);
}

zest_vec2 zest_WorldToScreenOrtho(const float point[3], float view_width, float view_height, zest_matrix4* projection, zest_matrix4* view) {
    float cx = projection->v[3].x + projection->v[0].x * (view->v[0].x * point[0] + view->v[1].x * point[1] + view->v[2].x * point[2] + view->v[3].x);
    float cy = projection->v[3].y + projection->v[1].y * (view->v[0].y * point[0] + view->v[1].y * point[1] + view->v[2].y * point[2] + view->v[3].y);

    return zest_Vec2Set((0.5f + 0.5f * -cx) * view_width, (0.5f + 0.5f * -cy) * view_height);
}

zest_matrix4 zest_Perspective(float fovy, float aspect, float zNear, float zFar) {
    float const tanHalfFovy = tanf(fovy / 2.f);
    zest_matrix4 result = zest_M4(0.f);
    result.v[0].x = 1.f / (aspect * tanHalfFovy);
    result.v[1].y = 1.f / (tanHalfFovy);
    result.v[2].z = zFar / (zNear - zFar);
    result.v[2].w = -1.f;
    result.v[3].z = -(zFar * zNear) / (zFar - zNear);
    return result;
}

zest_matrix4 zest_TransposeMatrix4(zest_matrix4* mat) {
    zest_matrix4 r;
    r.v[0].x = mat->v[0].x; r.v[0].y = mat->v[1].x; r.v[0].z = mat->v[2].x; r.v[0].w = mat->v[3].x;
    r.v[1].x = mat->v[0].y; r.v[1].y = mat->v[1].y; r.v[1].z = mat->v[2].y; r.v[1].w = mat->v[3].y;
    r.v[2].x = mat->v[0].z; r.v[2].y = mat->v[1].z; r.v[2].z = mat->v[2].z; r.v[2].w = mat->v[3].z;
    r.v[3].x = mat->v[0].w; r.v[3].y = mat->v[1].w; r.v[3].z = mat->v[2].w; r.v[3].w = mat->v[3].w;
    return r;
}

void zest_CalculateFrustumPlanes(zest_matrix4* view_matrix, zest_matrix4* proj_matrix, zest_vec4 planes[6]) {
    zest_matrix4 matrix = zest_MatrixTransform(view_matrix, proj_matrix);
    // Extracting frustum planes from view-projection matrix

    planes[zest_LEFT].x = matrix.v[0].w + matrix.v[0].x;
    planes[zest_LEFT].y = matrix.v[1].w + matrix.v[1].x;
    planes[zest_LEFT].z = matrix.v[2].w + matrix.v[2].x;
    planes[zest_LEFT].w = matrix.v[3].w + matrix.v[3].x;

    planes[zest_RIGHT].x = matrix.v[0].w - matrix.v[0].x;
    planes[zest_RIGHT].y = matrix.v[1].w - matrix.v[1].x;
    planes[zest_RIGHT].z = matrix.v[2].w - matrix.v[2].x;
    planes[zest_RIGHT].w = matrix.v[3].w - matrix.v[3].x;

    planes[zest_TOP].x = matrix.v[0].w - matrix.v[0].y;
    planes[zest_TOP].y = matrix.v[1].w - matrix.v[1].y;
    planes[zest_TOP].z = matrix.v[2].w - matrix.v[2].y;
    planes[zest_TOP].w = matrix.v[3].w - matrix.v[3].y;

    planes[zest_BOTTOM].x = matrix.v[0].w + matrix.v[0].y;
    planes[zest_BOTTOM].y = matrix.v[1].w + matrix.v[1].y;
    planes[zest_BOTTOM].z = matrix.v[2].w + matrix.v[2].y;
    planes[zest_BOTTOM].w = matrix.v[3].w + matrix.v[3].y;

    planes[zest_BACK].x = matrix.v[0].w + matrix.v[0].z;
    planes[zest_BACK].y = matrix.v[1].w + matrix.v[1].z;
    planes[zest_BACK].z = matrix.v[2].w + matrix.v[2].z;
    planes[zest_BACK].w = matrix.v[3].w + matrix.v[3].z;

    planes[zest_FRONT].x = matrix.v[0].w - matrix.v[0].z;
    planes[zest_FRONT].y = matrix.v[1].w - matrix.v[1].z;
    planes[zest_FRONT].z = matrix.v[2].w - matrix.v[2].z;
    planes[zest_FRONT].w = matrix.v[3].w - matrix.v[3].z;

    for (int i = 0; i < 6; ++i) {
        float length = sqrtf(planes[i].x * planes[i].x + planes[i].y * planes[i].y + planes[i].z * planes[i].z);
        planes[i].x /= length;
        planes[i].y /= length;
        planes[i].z /= length;
        planes[i].w /= length;
    }
}

zest_bool zest_IsPointInFrustum(const zest_vec4 planes[6], const float point[3]) {
    for (int i = 0; i < 6; i++) {
        if ((planes[i].x * point[0]) + (planes[i].y * point[1]) + (planes[i].z * point[2]) + planes[i].w < 0)
        {
            return ZEST_FALSE;
        }
    }
    // Point is inside the frustum
    return ZEST_TRUE;
}

zest_bool zest_IsSphereInFrustum(const zest_vec4 planes[6], const float point[3], float radius) {
    for (int i = 0; i < 6; i++) {
        if ((planes[i].x * point[0]) + (planes[i].y * point[1]) + (planes[i].z * point[2]) + planes[i].w <= -radius)
        {
            return ZEST_FALSE;
        }
    }
    // Point is inside the frustum
    return ZEST_TRUE;
}
//-- End camera and other helpers

float zest_Distance(float fromx, float fromy, float tox, float toy) {
    float w = tox - fromx;
    float h = toy - fromy;
    return sqrtf(w * w + h * h);
}

zest_uint zest_Pack16bit2SNorm(float x, float y) {
    union
    {
        signed short in[2];
        zest_uint out;
    } u;

    x = x * 32767.f;
    y = y * 32767.f;

    u.in[0] = (signed short)x;
    u.in[1] = (signed short)y;

    return u.out;
}

zest_u64 zest_Pack16bit4SNorm(float x, float y, float z, float w) {
    union
    {
        signed short in[4];
        zest_u64 out;
    } u;

    x = x * 32767.f;
    y = y * 32767.f;
    z = z * 32767.f;
    w = w * 32767.f;

    u.in[0] = (signed short)x;
    u.in[1] = (signed short)y;
    u.in[2] = (signed short)z;
    u.in[3] = (signed short)w;

    return u.out;
}

zest_uint zest_FloatToHalf(float f) {
    union {
        float f;
        zest_uint u;
    } u = { f };

    zest_uint sign = (u.u & 0x80000000) >> 16;
    zest_uint rest = u.u & 0x7FFFFFFF;

    if (rest >= 0x47800000) {  // Infinity or NaN
        return (uint16_t)(sign | 0x7C00 | (rest > 0x7F800000 ? 0x0200 : 0));
    }

    if (rest < 0x38800000) {  // Subnormal or zero
        rest = (rest & 0x007FFFFF) | 0x00800000;  // Add leading 1
        int shift = 113 - (rest >> 23);
        rest = (rest << 14) >> shift;
        return (uint16_t)(sign | rest);
    }

    zest_uint exponent = rest & 0x7F800000;
    zest_uint mantissa = rest & 0x007FFFFF;
    return (uint16_t)(sign | ((exponent - 0x38000000) >> 13) | (mantissa >> 13));
}

zest_u64 zest_Pack16bit4SFloat(float x, float y, float z, float w) {
    uint16_t hx = zest_FloatToHalf(x);
    uint16_t hy = zest_FloatToHalf(y);
    uint16_t hz = zest_FloatToHalf(z);
    uint16_t hw = zest_FloatToHalf(w);
    return ((zest_u64)hx) |
        ((zest_u64)hy << 16) |
        ((zest_u64)hz << 32) |
        ((zest_u64)hw << 48);
}

zest_uint zest_Pack16bit2SScaled(float x, float y, float max_value) {
    int16_t x_scaled = (int16_t)(x * 32767.0f / max_value);
    int16_t y_scaled = (int16_t)(y * 32767.0f / max_value);
    return ((zest_uint)x_scaled) | ((zest_uint)y_scaled << 16);
}

zest_u64 zest_Pack16bit4SScaled(float x, float y, float z, float w, float max_value_xy, float max_value_zw) {
    int16_t x_scaled = (int16_t)(x * 32767.0f / max_value_xy);
    int16_t y_scaled = (int16_t)(y * 32767.0f / max_value_xy);
    int16_t z_scaled = (int16_t)(z * 32767.0f / max_value_zw);
    int16_t w_scaled = (int16_t)(w * 32767.0f / max_value_zw);
    return ((zest_u64)x_scaled) | ((zest_u64)y_scaled << 16) | ((zest_u64)z_scaled << 32) | ((zest_u64)w_scaled << 48);
}

zest_u64 zest_Pack16bit4SScaledZWPacked(float x, float y, zest_uint zw, float max_value_xy) {
    int16_t x_scaled = (int16_t)(x * 32767.0f / max_value_xy);
    int16_t y_scaled = (int16_t)(y * 32767.0f / max_value_xy);
    return ((zest_u64)x_scaled) | ((zest_u64)y_scaled << 16) | ((zest_u64)zw << 32);
}

zest_uint zest_Pack8bit(float x, float y, float z) {
    union
    {
        signed char in[4];
        zest_uint out;
    } u;

    x = x * 127.f;
    y = y * 127.f;
    z = z * 127.f;

    u.in[0] = (signed char)x;
    u.in[1] = (signed char)y;
    u.in[2] = (signed char)z;
    u.in[3] = 0;

    return u.out;
}

zest_uint zest_Pack16bitStretch(float x, float y) {
    union
    {
        signed short in[2];
        zest_uint out;
    } u;

    x = x * 655.34f;
    y = y * 32767.f;

    u.in[0] = (signed short)x;
    u.in[1] = (signed short)y;

    return u.out;
}

zest_uint zest_Pack10bit(zest_vec3* v, zest_uint extra) {
    zest_vec3 converted = zest_ScaleVec3(*v, 511.f);
    zest_packed10bit result = ZEST__ZERO_INIT(zest_packed10bit);
    result.pack = 0;
    result.data.x = (zest_uint)converted.z;
    result.data.y = (zest_uint)converted.y;
    result.data.z = (zest_uint)converted.x;
    result.data.w = extra;
    return result.pack;
}

zest_uint zest_Pack8bitx3(zest_vec3* v) {
    zest_vec3 converted = zest_ScaleVec3(*v, 255.f);
    zest_packed8bit result = ZEST__ZERO_INIT(zest_packed8bit);
    result.pack = 0;
    result.data.x = (zest_uint)converted.z;
    result.data.y = (zest_uint)converted.y;
    result.data.z = (zest_uint)converted.x;
    result.data.w = 0;
    return result.pack;
}

zest_size zest_GetNextPower(zest_size n) {
    return 1ULL << (zloc__scan_reverse(n) + 1);
}

float zest_Radians(float degrees) { return degrees * 0.01745329251994329576923690768489f; }
float zest_Degrees(float radians) { return radians * 57.295779513082320876798154814105f; }

zest_vec4 zest_LerpVec4(zest_vec4* from, zest_vec4* to, float lerp) {
    return zest_AddVec4(zest_ScaleVec4(*to, lerp), zest_ScaleVec4(*from, (1.f - lerp)));
}

zest_vec3 zest_LerpVec3(zest_vec3* from, zest_vec3* to, float lerp) {
    return zest_AddVec3(zest_ScaleVec3(*to, lerp), zest_ScaleVec3(*from, (1.f - lerp)));
}

zest_vec2 zest_LerpVec2(zest_vec2* from, zest_vec2* to, float lerp) {
    return zest_AddVec2(zest_ScaleVec2(*to, lerp), zest_ScaleVec2(*from, (1.f - lerp)));
}

float zest_Lerp(float from, float to, float lerp) {
    return to * lerp + from * (1.f - lerp);
}
//  --End Math

void zest__register_platform(zest_platform_type type, zest__platform_setup callback) {
	zest__platform_setup_callbacks[type] = callback;
}

// Initialisation and destruction
zest_context zest_CreateContext(zest_device device, zest_window_data_t *window_data, zest_create_info_t* info) {
	ZEST_ASSERT_HANDLE(device);		//Not a valid device handle
	zloc_allocator *allocator = device->allocator;
    zest_context context = (zest_context)zloc_Allocate(allocator, sizeof(zest_context_t));
    *context = ZEST__ZERO_INIT(zest_context_t);
	context->magic = zest_INIT_MAGIC(zest_struct_type_context);

	context->device = device;

	context->create_info = *info;
    context->backend = (zest_context_backend)device->platform->new_context_backend(context);
    context->fence_wait_timeout_ns = info->fence_wait_timeout_ms * 1000 * 1000;
	context->window_data = *window_data;
	zest_bool result = zest__initialise_context(context, info);
	if (result != ZEST_TRUE) {
		ZEST_PRINT("Unable to initialise the renderer. Check the log for details.");
		zest__destroy(context);
		return NULL;
	}
    return context;
}

zest_device_builder zest__begin_device_builder() {
    void* memory_pool = ZEST__ALLOCATE_POOL(zloc__MEGABYTE(1));
	zloc_allocator *allocator = zloc_InitialiseAllocatorWithPool(memory_pool, zloc__MEGABYTE(1));
	zest_device_builder builder = (zest_device_builder)ZEST__NEW(allocator, zest_device_builder);
	*builder = ZEST__ZERO_INIT(zest_device_builder_t);
	builder->magic = zest_INIT_MAGIC(zest_struct_type_device_builder);
	builder->platform = zest_platform_vulkan;
	builder->allocator = allocator;
	builder->memory_pool_size = zloc__MEGABYTE(256);
	builder->thread_count = zest_GetDefaultThreadCount();
	builder->log_path = "./";
	return builder;
}

void zest_AddDeviceBuilderExtension(zest_device_builder builder, const char *extension_name) {
	ZEST_ASSERT_HANDLE(builder);	//Not a valid zest_device_builder handle. Make sure you call zest_Begin[Platform]DeviceBuilder
	size_t len = strlen(extension_name) + 1;
	char* name_copy = (char*)ZEST__ALLOCATE(builder->allocator, len);
	ZEST_ASSERT(name_copy);	//Unable to allocate enough space for the extension name
	memcpy(name_copy, extension_name, len);
	zest_vec_push(builder->allocator, builder->required_instance_extensions, name_copy);
}

void zest_AddDeviceBuilderExtensions(zest_device_builder builder, const char **extension_names, int count) {
	ZEST_ASSERT_HANDLE(builder);	//Not a valid zest_device_builder handle. Make sure you call zest_Begin[Platform]DeviceBuilder
	for (int i = 0; i != count; ++i) {
		const char *extension_name = extension_names[i];
		size_t len = strlen(extension_name) + 1;
		char* name_copy = (char*)ZEST__ALLOCATE(builder->allocator, len);
		ZEST_ASSERT(name_copy);	//Unable to allocate enough space for the extension name
		memcpy(name_copy, extension_name, len);
		zest_vec_push(builder->allocator, builder->required_instance_extensions, name_copy);
	}
}

void zest_AddDeviceBuilderValidation(zest_device_builder builder) {
    ZEST__FLAG(builder->flags, zest_init_flag_enable_validation_layers);
	ZEST__FLAG(builder->flags, zest_init_flag_enable_validation_layers_with_sync);
}

void zest_AddDeviceBuilderFullValidation(zest_device_builder builder) {
    ZEST__FLAG(builder->flags, zest_init_flag_enable_validation_layers);
	ZEST__FLAG(builder->flags, zest_init_flag_enable_validation_layers_with_sync);
	ZEST__FLAG(builder->flags, zest_init_flag_enable_validation_layers_with_best_practices);
}

void zest_DeviceBuilderLogToConsole(zest_device_builder builder) {
	ZEST__FLAG(builder->flags, zest_init_flag_log_validation_errors_to_console);
}

void zest_DeviceBuilderLogToMemory(zest_device_builder builder) {
	ZEST__FLAG(builder->flags, zest_init_flag_log_validation_errors_to_memory);
}

void zest_DeviceBuilderLogPath(zest_device_builder builder, const char *log_path) {
	builder->log_path = log_path;
}

void zest_SetDeviceBuilderMemoryPoolSize(zest_device_builder builder, zest_size size) {
	ZEST_ASSERT_HANDLE(builder);	//Not a valid zest_device_builder handle. Make sure you call zest_Begin[Platform]DeviceBuilder
	ZEST_ASSERT(size > zloc__MEGABYTE(8));	//Size for the memory pool must be greater than 8 megabytes
	builder->memory_pool_size = size;
}

zest_device zest_EndDeviceBuilder(zest_device_builder builder) {
	ZEST_ASSERT_HANDLE(builder);	//Not a valid zest_device_builder handle. Make sure you call zest_Begin[Platform]DeviceBuilder

    void* memory_pool = ZEST__ALLOCATE_POOL(builder->memory_pool_size);
	ZEST_ASSERT(memory_pool);    //unable to allocate initial memory pool

    zloc_allocator* allocator = zloc_InitialiseAllocatorWithPool(memory_pool, builder->memory_pool_size);

    zest_device device = (zest_device)zloc_Allocate(allocator, sizeof(zest_device_t));
	allocator->user_data = device;

	*device = ZEST__ZERO_INIT(zest_device_t);
    device->allocator = allocator;
	device->platform = (zest_platform)zloc_Allocate(allocator, sizeof(zest_platform_t));
	device->init_flags = builder->flags;

    device->thread_count = builder->thread_count;
    if (builder->thread_count > 0) {
        zest__initialise_thread_queues(&device->thread_queues);
        for (int i = 0; i < builder->thread_count; i++) {
            if (!zest__create_worker_thread(device, i)) {
                ZEST_ASSERT(0);     //Unable to create thread!
            }
        }
    }

    device->magic = zest_INIT_MAGIC(zest_struct_type_device);
    device->allocator = allocator;
    device->memory_pools[0] = memory_pool;
    device->memory_pool_sizes[0] = builder->memory_pool_size;
    device->memory_pool_count = 1;
    device->setup_info = *builder;
	device->setup_info.allocator = NULL;
    void *scratch_memory = ZEST__ALLOCATE(device->allocator, zloc__MEGABYTE(1));
    device->scratch_arena = zloc_InitialiseLinearAllocator(scratch_memory, zloc__MEGABYTE(1));
    if (builder->log_path) {
        zest_SetErrorLogPath(device, builder->log_path);
    }

	switch (builder->platform) {
		case zest_platform_vulkan: {
			if (!zest__initialise_vulkan_device(device, builder)) {
				ZEST__FREE_POOL(memory_pool);
				return NULL;
			}
			break;
		}
	}
	ZEST__FREE_POOL(builder->allocator);
	return device;
}

zest_bool zest__initialise_vulkan_device(zest_device device, zest_device_builder info) {

	if (zest__platform_setup_callbacks[zest_platform_vulkan]) {
		zest__platform_setup_callbacks[zest_platform_vulkan](device->platform);
	} else {
		zloc_Free(device->allocator, device->platform);
		zloc_Free(device->allocator, device);
		ZEST_PRINT("No platform set up function found. Make sure you called the appropriate function for the platform that you want to use like zest_BeginVulkanDevice()");
		return ZEST_FALSE;
	}

    device->backend = (zest_device_backend)device->platform->new_device_backend(device);

	zest_vec_foreach(i, info->required_instance_extensions) {
		char *extension = (char*)info->required_instance_extensions[i];
		zest_vec_push(device->allocator, device->extensions, extension);
	}

	if (device->platform->initialise_device(device)) {
		return ZEST_TRUE;
	}

	zest__cleanup_device(device);
	return ZEST_FALSE;
}

zest_bool zest_BeginFrame(zest_context context) {
	ZEST_ASSERT(ZEST__NOT_FLAGGED(context->flags, zest_context_flag_swap_chain_was_acquired), "You have called zest_BeginFrame but a swap chain image has already been acquired. Make sure that you call zest_EndFrame before you loop around to zest_BeginFrame again.");
	zest_fence_status fence_wait_result = zest__main_loop_fence_wait(context);
	if (fence_wait_result == zest_fence_status_success) {
	} else if (fence_wait_result == zest_fence_status_timeout) {
		ZEST_PRINT("Fence wait timed out.");
		ZEST__FLAG(context->flags, zest_context_flag_critical_error);
		return ZEST_FALSE;
	} else {
		ZEST_PRINT("Critical error when waiting for the main loop fence.");
		ZEST__FLAG(context->flags, zest_context_flag_critical_error);
		return ZEST_FALSE;
	}

	ZEST__UNFLAG(context->flags, zest_context_flag_swap_chain_was_acquired);

	zest__do_scheduled_tasks(context);

	if (!context->device->platform->acquire_swapchain_image(context->swapchain)) {
		zest__recreate_swapchain(context);
		ZEST__UNFLAG(context->flags, zest_context_flag_building_frame_graph);
		ZEST_PRINT("Unable to acquire the swap chain!");
		return false;
	}
	ZEST__FLAG(context->flags, zest_context_flag_swap_chain_was_acquired);
	ZEST__UNFLAG(context->swapchain->flags, zest_swapchain_flag_was_recreated);

	return fence_wait_result == zest_fence_status_success;
}

void zest_EndFrame(zest_context context) {
	zest_frame_graph_flags flags = 0;
	ZEST__UNFLAG(context->flags, zest_context_flag_work_was_submitted);
    if (zest_vec_size(context->frame_graphs)) {
        zest_vec_foreach(i, context->frame_graphs) {
            zest_frame_graph frame_graph = context->frame_graphs[i];
            flags |= frame_graph->flags & zest_frame_graph_present_after_execute;

            zest_frame_graph_builder_t builder = ZEST__ZERO_INIT(zest_frame_graph_builder_t);
            zest__frame_graph_builder = &builder;
            zest__frame_graph_builder->context = context;
            zest__frame_graph_builder->frame_graph = frame_graph;
            zest__frame_graph_builder->current_pass = 0;
            zest__execute_frame_graph(context, frame_graph, ZEST_FALSE);
            zest__frame_graph_builder = NULL;
        }
    } else {
        ZEST__REPORT(context, zest_report_no_frame_graphs_to_execute, "WARNING: There were no frame graphs to execute this frame. Make sure that you call zest_QueueFrameGraphForExecution after building or fetching a cached frame graph.");
    }

	if (ZEST_VALID_HANDLE(context->swapchain) && ZEST__FLAGGED(flags, zest_frame_graph_present_after_execute)) {
		zest_bool presented = context->device->platform->present_frame(context);
		context->previous_fif = context->current_fif;
		context->current_fif = (context->current_fif + 1) % ZEST_MAX_FIF;
		if(!presented) {
			zest__recreate_swapchain(context);
		}
	}
	//Cover some cases where a frame graph wasn't created or it was but there was nothing render etc., to make sure
	//that the fence is always signalled and another frame can happen
	if (ZEST__FLAGGED(context->flags, zest_context_flag_swap_chain_was_acquired)) {
		if (ZEST__NOT_FLAGGED(context->flags, zest_context_flag_work_was_submitted)) {
			if (context->device->platform->dummy_submit_for_present_only(context)) {
				zest_bool presented = context->device->platform->present_frame(context);
				context->previous_fif = context->current_fif;
				context->current_fif = (context->current_fif + 1) % ZEST_MAX_FIF;
				if (!presented) {
					zest__recreate_swapchain(context);
				}
			}
		}
	}
	ZEST__UNFLAG(context->flags, zest_context_flag_swap_chain_was_acquired);
}

void zest_DestroyContext(zest_context context) {
    zest__destroy(context);
}

void zest_SetCreateInfo(zest_context context, zest_create_info_t *info) {
    context->create_info = *info;
}

void zest_SetContextUserData(zest_context context, void *user_data) {
	ZEST_ASSERT_HANDLE(context);		//Not a valid context handle
	context->user_data = user_data;
}

void zest_ResetRenderer(zest_context context, zest_window_data_t *window_data) {

    zest_WaitForIdleDevice(context);

    zest__cleanup_context(context);

	if (window_data) {
		context->window_data = *window_data;
	}
    context->backend = (zest_context_backend)context->device->platform->new_context_backend(context);
    zest__initialise_context(context, &context->create_info);
}

//-- End Initialisation and destruction

/*
Functions that create a vulkan device
*/


void zest_AddInstanceExtension(zest_device device, char* extension) {
    zest_vec_push(device->allocator, device->extensions, extension);
}

void zest__set_default_pool_sizes(zest_device device) {
    zest_buffer_usage_t usage = ZEST__ZERO_INIT(zest_buffer_usage_t);
    //Images stored on device use share a single pool as far as I know
    //But not true was having issues with this. An image buffer can share the same usage properties but have different alignment requirements
    //So they will get separate pools
    //Depth buffers
    usage.image_flags = zest_image_usage_depth_stencil_attachment_bit;
    usage.property_flags = zest_memory_property_device_local_bit;
    zest_SetDeviceImagePoolSize(device, "Depth Buffer", usage.image_flags, usage.property_flags, 1024, zloc__MEGABYTE(64));

    //General Textures
    usage.image_flags = zest_image_usage_transfer_dst_bit | zest_image_usage_sampled_bit;
    usage.property_flags = zest_memory_property_device_local_bit;
    zest_SetDeviceImagePoolSize(device, "Texture Buffer", usage.image_flags, usage.property_flags, 1024, zloc__MEGABYTE(64));

    usage.image_flags = zest_image_usage_transfer_src_bit | zest_image_usage_transfer_dst_bit | zest_image_usage_sampled_bit;
    usage.property_flags = zest_memory_property_device_local_bit;
    zest_SetDeviceImagePoolSize(device, "Texture Buffer Mipped", usage.image_flags, usage.property_flags, 1024, zloc__MEGABYTE(64));

    //Storage Textures For Writing/Reading from
    usage.image_flags = zest_image_usage_transfer_src_bit | zest_image_usage_transfer_dst_bit | zest_image_usage_storage_bit | zest_image_usage_sampled_bit;
    usage.property_flags = zest_memory_property_device_local_bit;
    zest_SetDeviceImagePoolSize(device, "Texture Read/Write", usage.image_flags, usage.property_flags, 1024, zloc__MEGABYTE(64));

    //Render targets
    usage.image_flags = zest_image_usage_transfer_src_bit | zest_image_usage_transfer_dst_bit | zest_image_usage_sampled_bit | zest_image_usage_color_attachment_bit;
    usage.property_flags = zest_memory_property_device_local_bit;
    zest_SetDeviceImagePoolSize(device, "Render Target", usage.image_flags, usage.property_flags, 1024, zloc__MEGABYTE(64));

    //Uniform buffers
    usage.image_flags = 0;
    usage.buffer_usage_flags = zest_buffer_usage_uniform_buffer_bit;
    usage.property_flags = zest_memory_property_host_visible_bit | zest_memory_property_host_coherent_bit;
    zest_SetDeviceBufferPoolSize(device, "Uniform Buffers", usage.buffer_usage_flags, usage.property_flags, 64, zloc__MEGABYTE(1));

    //Indirect draw buffers that are host visible
    usage.image_flags = 0;
    usage.buffer_usage_flags = zest_buffer_usage_indirect_buffer_bit | zest_buffer_usage_transfer_dst_bit;
    usage.property_flags = zest_memory_property_host_visible_bit | zest_memory_property_host_coherent_bit;
    zest_SetDeviceBufferPoolSize(device, "Host Indirect Draw Buffers", usage.buffer_usage_flags, usage.property_flags, 64, zloc__MEGABYTE(1));

    //Indirect draw buffers
    usage.image_flags = 0;
    usage.buffer_usage_flags = zest_buffer_usage_indirect_buffer_bit | zest_buffer_usage_transfer_dst_bit;
    usage.property_flags = zest_memory_property_device_local_bit;
    zest_SetDeviceBufferPoolSize(device, "Host Indirect Draw Buffers", usage.buffer_usage_flags, usage.property_flags, 64, zloc__MEGABYTE(1));

    //Staging buffer
    usage.buffer_usage_flags = zest_buffer_usage_transfer_dst_bit | zest_buffer_usage_transfer_src_bit;
    usage.property_flags = zest_memory_property_host_visible_bit | zest_memory_property_host_coherent_bit;
    zest_SetDeviceBufferPoolSize(device, "Host Staging Buffers", usage.buffer_usage_flags, usage.property_flags, zloc__KILOBYTE(2), zloc__MEGABYTE(32));

    //Vertex buffer
    usage.buffer_usage_flags = zest_buffer_usage_transfer_dst_bit | zest_buffer_usage_vertex_buffer_bit;
    usage.property_flags = zest_memory_property_device_local_bit;
    zest_SetDeviceBufferPoolSize(device, "Vertex Buffers", usage.buffer_usage_flags, usage.property_flags, zloc__KILOBYTE(1), zloc__MEGABYTE(4));

    //CPU Visible Vertex buffer
    usage.buffer_usage_flags = zest_buffer_usage_transfer_dst_bit | zest_buffer_usage_vertex_buffer_bit;
    usage.property_flags = zest_memory_property_host_visible_bit | zest_memory_property_device_local_bit;
    zest_SetDeviceBufferPoolSize(device, "CPU Visible Vertex Buffers", usage.buffer_usage_flags, usage.property_flags, zloc__KILOBYTE(1), zloc__MEGABYTE(32));

    //Storage buffer
    usage.buffer_usage_flags = zest_buffer_usage_transfer_dst_bit | zest_buffer_usage_storage_buffer_bit;
    usage.property_flags = zest_memory_property_device_local_bit;
    zest_SetDeviceBufferPoolSize(device, "Storage Buffers", usage.buffer_usage_flags, usage.property_flags, zloc__KILOBYTE(1), zloc__MEGABYTE(4));

    //CPU Visible Storage buffer
    usage.buffer_usage_flags = zest_buffer_usage_transfer_dst_bit | zest_buffer_usage_storage_buffer_bit;
    usage.property_flags = zest_memory_property_host_visible_bit | zest_memory_property_device_local_bit;
    zest_SetDeviceBufferPoolSize(device, "CPU Visible Storage Buffers", usage.buffer_usage_flags, usage.property_flags, zloc__KILOBYTE(1), zloc__MEGABYTE(32));

    //Index buffer
    usage.buffer_usage_flags = zest_buffer_usage_transfer_dst_bit | zest_buffer_usage_index_buffer_bit;
    usage.property_flags = zest_memory_property_device_local_bit;
    zest_SetDeviceBufferPoolSize(device, "Index Buffers", usage.buffer_usage_flags, usage.property_flags, zloc__KILOBYTE(1), zloc__MEGABYTE(4));

    //CPU Visible Index buffer
    usage.buffer_usage_flags = zest_buffer_usage_transfer_dst_bit | zest_buffer_usage_index_buffer_bit;
    usage.property_flags = zest_memory_property_host_visible_bit | zest_memory_property_device_local_bit;
    zest_SetDeviceBufferPoolSize(device, "Index Buffers", usage.buffer_usage_flags, usage.property_flags, zloc__KILOBYTE(1), zloc__MEGABYTE(32));

    //Vertex buffer with storage flag
    usage.buffer_usage_flags = zest_buffer_usage_transfer_dst_bit | zest_buffer_usage_vertex_buffer_bit | zest_buffer_usage_storage_buffer_bit;
    usage.property_flags = zest_memory_property_device_local_bit;
    zest_SetDeviceBufferPoolSize(device, "Vertex Buffers", usage.buffer_usage_flags, usage.property_flags, zloc__KILOBYTE(1), zloc__MEGABYTE(4));

    //Index buffer with storage flag
    usage.buffer_usage_flags = zest_buffer_usage_transfer_dst_bit | zest_buffer_usage_index_buffer_bit | zest_buffer_usage_storage_buffer_bit;
    usage.property_flags = zest_memory_property_device_local_bit;
    zest_SetDeviceBufferPoolSize(device, "Index Buffers", usage.buffer_usage_flags, usage.property_flags, zloc__KILOBYTE(1), zloc__MEGABYTE(4));
    ZEST_APPEND_LOG(device->log_path.str, "Set device pool sizes");
}

/*
End of Device creation functions
*/

void zest__end_thread(zest_work_queue_t *queue, void *data) {
    return;
}

void zest__destroy(zest_context context) {
    zest_PrintReports(context);
    zest_WaitForIdleDevice(context);
    context->device->thread_queues.end_all_threads = true;
    zest_work_queue_t end_queue = ZEST__ZERO_INIT(zest_work_queue_t);
    zest_uint thread_count = context->device->thread_count;
    while (thread_count > 0) {
        zest__add_work_queue_entry(context, 0, zest__end_thread);
        zest__complete_all_work(&end_queue);
        thread_count--;
    }
    for (int i = 0; i != context->device->thread_count; ++i) {
        zest__cleanup_thread(context, i);
    }
    zloc_allocator *allocator = context->device->allocator;
    zest__cleanup_context(context);
    zest__cleanup_device(context->device);
    zest_ResetValidationErrors(context);
    ZEST__FREE(context->device->allocator, context->device);
    ZEST__FREE(context->device->allocator, context->device->platform);
    ZEST__FREE(context->device->allocator, context);
	zloc_pool_stats_t stats = zloc_CreateMemorySnapshot(zloc__first_block_in_pool(zloc_GetPool(context->device->allocator)));
    if (stats.used_blocks > 0) {
        ZEST_PRINT("There are still used memory blocks in Zest, this indicates a memory leak and a possible bug in the Zest Renderer. There should be no used blocks after Zest has shutdown. Check the type of allocation in the list below and check to make sure you're freeing those objects.");
        zest_PrintMemoryBlocks(context, zloc__first_block_in_pool(zloc_GetPool(context->device->allocator)), 1, zest_platform_none, zest_command_none);
    } else {
        ZEST_PRINT("Successful shutdown of Zest.");
    }
}

void zest__do_scheduled_tasks(zest_context context) {
    zloc_ResetLinearAllocator(context->frame_graph_allocator[context->current_fif]);
    context->frame_graphs = 0;

    zest_vec_foreach(i, context->device->queues) {
        zest_queue queue = context->device->queues[i];
		context->device->platform->reset_queue_command_pool(context, queue);
		queue->next_buffer = 0;
    }

    if (zest_vec_size(context->device->deferred_resource_freeing_list.resources[context->current_fif])) {
        zest_vec_foreach(i, context->device->deferred_resource_freeing_list.resources[context->current_fif]) {
            void *handle = context->device->deferred_resource_freeing_list.resources[context->current_fif][i];
            zest__free_handle(handle);
        }
		zest_vec_clear(context->device->deferred_resource_freeing_list.resources[context->current_fif]);
    }

    if (zest_vec_size(context->device->deferred_resource_freeing_list.binding_indexes[context->current_fif])) {
        zest_vec_foreach(i, context->device->deferred_resource_freeing_list.binding_indexes[context->current_fif]) {
            zest_binding_index_for_release_t index = context->device->deferred_resource_freeing_list.binding_indexes[context->current_fif][i];
            zest__release_bindless_index(index.layout, index.binding_number, index.binding_index);
        }
		zest_vec_clear(context->device->deferred_resource_freeing_list.binding_indexes[context->current_fif]);
    }
    
    if (zest_vec_size(context->device->deferred_resource_freeing_list.images[context->current_fif])) {
        zest_vec_foreach(i, context->device->deferred_resource_freeing_list.images[context->current_fif]) {
            zest_image image = &context->device->deferred_resource_freeing_list.images[context->current_fif][i];
            context->device->platform->cleanup_image_backend(image);
            zest_FreeBuffer(image->buffer);
            if (image->default_view) {
				context->device->platform->cleanup_image_view_backend(image->default_view);
            }
        }
		zest_vec_clear(context->device->deferred_resource_freeing_list.images[context->current_fif]);
    }

    if (zest_vec_size(context->device->deferred_resource_freeing_list.views[context->current_fif])) {
        zest_vec_foreach(i, context->device->deferred_resource_freeing_list.views[context->current_fif]) {
            zest_image_view view = &context->device->deferred_resource_freeing_list.views[context->current_fif][i];
            context->device->platform->cleanup_image_view_backend(view);
        }
		zest_vec_clear(context->device->deferred_resource_freeing_list.views[context->current_fif]);
    }

    if (zest_vec_size(context->device->deferred_resource_freeing_list.view_arrays[context->current_fif])) {
        zest_vec_foreach(i, context->device->deferred_resource_freeing_list.view_arrays[context->current_fif]) {
            zest_image_view_array view_array = &context->device->deferred_resource_freeing_list.view_arrays[context->current_fif][i];
            context->device->platform->cleanup_image_view_array_backend(view_array);
        }
		zest_vec_clear(context->device->deferred_resource_freeing_list.view_arrays[context->current_fif]);
    }

    zest_vec_foreach(i, context->device->deferred_staging_buffers_for_freeing) {
        zest_buffer staging_buffer = context->device->deferred_staging_buffers_for_freeing[i];
		zest_FreeBuffer(staging_buffer);
    }
    zest_vec_clear(context->device->deferred_staging_buffers_for_freeing);
}

zest_fence_status zest__main_loop_fence_wait(zest_context context) {
	if (context->fence_count[context->current_fif]) {
		zest_millisecs start_time = zest_Millisecs();
		zest_uint retry_count = 0;
		while(1) {
			zest_fence_status result = context->device->platform->wait_for_renderer_fences(context);
            if (result == zest_fence_status_success) {
                break;
            } else if (result == zest_fence_status_timeout) {
				zest_millisecs total_wait_time = zest_Millisecs() - start_time;
				if (context->fence_wait_timeout_callback) {
                    if (context->fence_wait_timeout_callback(total_wait_time, retry_count++, context->user_data)) {
                        continue;
                    } else {
                        return result;
                    }
				} else {
					if (total_wait_time > context->create_info.max_fence_timeout_ms) {
                        return result;
					}
				}
			} else {
                return result;
			}
		}
		context->device->platform->reset_renderer_fences(context);
		context->fence_count[context->current_fif] = 0;
	}
	return zest_fence_status_success;
}

// Enum_to_string_functions - Helper functions to convert enums to strings
const char *zest__image_layout_to_string(zest_image_layout layout) {
    switch (layout) {
    case zest_image_layout_undefined: return "UNDEFINED"; break;
    case zest_image_layout_general: return "GENERAL"; break;
    case zest_image_layout_color_attachment_optimal: return "COLOR_ATTACHMENT_OPTIMAL"; break;
    case zest_image_layout_depth_stencil_attachment_optimal: return "DEPTH_STENCIL_ATTACHMENT_OPTIMAL"; break;
    case zest_image_layout_depth_stencil_read_only_optimal: return "DEPTH_STENCIL_READ_ONLY_OPTIMAL"; break;
    case zest_image_layout_shader_read_only_optimal: return "SHADER_READ_ONLY_OPTIMAL"; break;
    case zest_image_layout_transfer_src_optimal: return "TRANSFER_SRC_OPTIMAL"; break;
    case zest_image_layout_transfer_dst_optimal: return "TRANSFER_DST_OPTIMAL"; break;
    case zest_image_layout_preinitialized: return "PREINITIALIZED"; break;
    case zest_image_layout_depth_read_only_stencil_attachment_optimal: return "DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL"; break;
    case zest_image_layout_depth_attachment_stencil_read_only_optimal: return "DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL"; break;
    case zest_image_layout_depth_attachment_optimal: return "DEPTH_ATTACHMENT_OPTIMAL"; break;
    case zest_image_layout_depth_read_only_optimal: return "DEPTH_READ_ONLY_OPTIMAL"; break;
    case zest_image_layout_stencil_attachment_optimal: return "STENCIL_ATTACHMENT_OPTIMAL"; break;
    case zest_image_layout_stencil_read_only_optimal: return "STENCIL_READ_ONLY_OPTIMAL"; break;
    case zest_image_layout_read_only_optimal: return "READ_ONLY_OPTIMAL"; break;
    case zest_image_layout_attachment_optimal: return "ATTACHMENT_OPTIMAL"; break;
    default: return "Unknown Layout";
    }
}

zest_text_t zest__access_flags_to_string(zest_context context, zest_access_flags flags) {
    zest_text_t string = ZEST__ZERO_INIT(zest_text_t);
    if (!flags) {
        zest_AppendTextf(context->device->allocator, &string, "%s", "NONE");
        return string;
    }
    zloc_size flags_field = flags;
    while (flags_field) {
        if (zest_TextSize(&string)) {
            zest_AppendTextf(context->device->allocator, &string, ", ");
        }
        zest_uint index = zloc__scan_forward(flags_field);
        switch (1ull << index) {
        case zest_access_indirect_command_read_bit: zest_AppendTextf(context->device->allocator, &string, "%s", "INDIRECT_COMMAND_READ_BIT"); break;
        case zest_access_index_read_bit: zest_AppendTextf(context->device->allocator, &string, "%s", "INDEX_READ_BIT"); break;
        case zest_access_vertex_attribute_read_bit: zest_AppendTextf(context->device->allocator, &string, "%s", "VERTEX_ATTRIBUTE_READ_BIT"); break;
        case zest_access_uniform_read_bit: zest_AppendTextf(context->device->allocator, &string, "%s", "UNIFORM_READ_BIT"); break;
        case zest_access_input_attachment_read_bit: zest_AppendTextf(context->device->allocator, &string, "%s", "INPUT_ATTACHMENT_READ_BIT"); break;
        case zest_access_shader_read_bit: zest_AppendTextf(context->device->allocator, &string, "%s", "SHADER_READ_BIT"); break;
        case zest_access_shader_write_bit: zest_AppendTextf(context->device->allocator, &string, "%s", "SHADER_WRITE_BIT"); break;
        case zest_access_color_attachment_read_bit: zest_AppendTextf(context->device->allocator, &string, "%s", "COLOR_ATTACHMENT_READ_BIT"); break;
        case zest_access_color_attachment_write_bit: zest_AppendTextf(context->device->allocator, &string, "%s", "COLOR_ATTACHMENT_WRITE_BIT"); break;
        case zest_access_depth_stencil_attachment_read_bit: zest_AppendTextf(context->device->allocator, &string, "%s", "DEPTH_STENCIL_ATTACHMENT_READ_BIT"); break;
        case zest_access_depth_stencil_attachment_write_bit: zest_AppendTextf(context->device->allocator, &string, "%s", "DEPTH_STENCIL_ATTACHMENT_WRITE_BIT"); break;
        case zest_access_transfer_read_bit: zest_AppendTextf(context->device->allocator, &string, "%s", "TRANSFER_READ_BIT"); break;
        case zest_access_transfer_write_bit: zest_AppendTextf(context->device->allocator, &string, "%s", "TRANSFER_WRITE_BIT"); break;
        case zest_access_host_read_bit: zest_AppendTextf(context->device->allocator, &string, "%s", "HOST_READ_BIT"); break;
        case zest_access_host_write_bit: zest_AppendTextf(context->device->allocator, &string, "%s", "HOST_WRITE_BIT"); break;
        case zest_access_memory_read_bit: zest_AppendTextf(context->device->allocator, &string, "%s", "MEMORY_READ_BIT"); break;
        case zest_access_memory_write_bit: zest_AppendTextf(context->device->allocator, &string, "%s", "MEMORY_WRITE_BIT"); break;
        case zest_access_none: zest_AppendTextf(context->device->allocator, &string, "%s", "NONE"); break;
        default: zest_AppendTextf(context->device->allocator, &string, "%s", "Unknown Access Flags"); break;
        }
        flags_field &= ~(1ull << index);
    }
    return string;
}

zest_text_t zest__pipeline_stage_flags_to_string(zest_context context, zest_pipeline_stage_flags flags) {
    zest_text_t string = ZEST__ZERO_INIT(zest_text_t);
    if (!flags) {
        zest_AppendTextf(context->device->allocator, &string, "%s", "NONE");
        return string;
    }
    zloc_size flags_field = flags;
    while (flags_field) {
        if (zest_TextSize(&string)) {
            zest_AppendTextf(context->device->allocator, &string, ", ");
        }
        zest_uint index = zloc__scan_forward(flags_field);
        switch (1ull << index) {
        case zest_pipeline_stage_top_of_pipe_bit: zest_AppendTextf(context->device->allocator, &string, "%s", "TOP_OF_PIPE_BIT"); break;
        case zest_pipeline_stage_draw_indirect_bit: zest_AppendTextf(context->device->allocator, &string, "%s", "DRAW_INDIRECT_BIT"); break;
        case zest_pipeline_stage_vertex_input_bit: zest_AppendTextf(context->device->allocator, &string, "%s", "VERTEX_INPUT_BIT"); break;
        case zest_pipeline_stage_vertex_shader_bit: zest_AppendTextf(context->device->allocator, &string, "%s", "VERTEX_SHADER_BIT"); break;
        case zest_pipeline_stage_tessellation_control_shader_bit: zest_AppendTextf(context->device->allocator, &string, "%s", "TESSELLATION_CONTROL_SHADER_BIT"); break;
        case zest_pipeline_stage_tessellation_evaluation_shader_bit: zest_AppendTextf(context->device->allocator, &string, "%s", "TESSELLATION_EVALUATION_SHADER_BIT"); break;
        case zest_pipeline_stage_geometry_shader_bit: zest_AppendTextf(context->device->allocator, &string, "%s", "GEOMETRY_SHADER_BIT"); break;
        case zest_pipeline_stage_fragment_shader_bit: zest_AppendTextf(context->device->allocator, &string, "%s", "FRAGMENT_SHADER_BIT"); break;
        case zest_pipeline_stage_early_fragment_tests_bit: zest_AppendTextf(context->device->allocator, &string, "%s", "EARLY_FRAGMENT_TESTS_BIT"); break;
        case zest_pipeline_stage_late_fragment_tests_bit: zest_AppendTextf(context->device->allocator, &string, "%s", "LATE_FRAGMENT_TESTS_BIT"); break;
        case zest_pipeline_stage_color_attachment_output_bit: zest_AppendTextf(context->device->allocator, &string, "%s", "COLOR_ATTACHMENT_OUTPUT_BIT"); break;
        case zest_pipeline_stage_compute_shader_bit: zest_AppendTextf(context->device->allocator, &string, "%s", "COMPUTE_SHADER_BIT"); break;
        case zest_pipeline_stage_transfer_bit: zest_AppendTextf(context->device->allocator, &string, "%s", "TRANSFER_BIT"); break;
        case zest_pipeline_stage_bottom_of_pipe_bit: zest_AppendTextf(context->device->allocator, &string, "%s", "BOTTOM_OF_PIPE_BIT"); break;
        case zest_pipeline_stage_host_bit: zest_AppendTextf(context->device->allocator, &string, "%s", "HOST_BIT"); break;
        case zest_pipeline_stage_all_graphics_bit: zest_AppendTextf(context->device->allocator, &string, "%s", "ALL_GRAPHICS_BIT"); break;
        case zest_pipeline_stage_all_commands_bit: zest_AppendTextf(context->device->allocator, &string, "%s", "ALL_COMMANDS_BIT"); break;
        case zest_pipeline_stage_none: zest_AppendTextf(context->device->allocator, &string, "%s", "NONE"); break;
        default: zest_AppendTextf(context->device->allocator, &string, "%s", "Unknown Pipeline Stage"); break;
        }
        flags_field &= ~(1ull << index);
    }
    return string;
}

// --Threading related funcitons
unsigned int zest_HardwareConcurrencySafe(void) {
    unsigned int count = zest_HardwareConcurrency();
    return count > 0 ? count : 1;
}

unsigned int zest_GetDefaultThreadCount(void) {
    unsigned int count = zest_HardwareConcurrency();
    return count > 1 ? count - 1 : 1;
}

#ifdef _WIN32
unsigned WINAPI zest__thread_worker(void *arg) {
    zest_queue_processor_t *queue_processor = (zest_queue_processor_t *)arg;
    while (!zest__do_next_work_queue(queue_processor)) {
        // Continue processing
    }
    return 0;
}
#else
void *zest__thread_worker(void *arg) {
    zest_queue_processor_t *queue_processor = (zest_queue_processor_t *)arg;
    while (!zest__do_next_work_queue(queue_processor)) {
        // Continue processing
    }
    return 0;
}
#endif

void zest__cleanup_thread(zest_context context, int thread_index) {
#ifdef _WIN32
    if (context->device->threads[thread_index]) {
        WaitForSingleObject(context->device->threads[thread_index], INFINITE);
        CloseHandle(context->device->threads[thread_index]);
        context->device->threads[thread_index] = NULL;
    }
#else
    pthread_join(context->device->threads[thread_index], NULL);
#endif
}

bool zest__create_worker_thread(zest_device device, int thread_index) {
#ifdef _WIN32
    device->threads[thread_index] = (HANDLE)_beginthreadex(
        NULL,
        0,
        zest__thread_worker,
        &device->thread_queues,
        0,
        NULL
    );
    return device->threads[thread_index] != NULL;
#else
    return pthread_create(
        &device->threads[thread_index],
        NULL,
        zest__thread_worker,
        &storage->thread_queues
    ) == 0;
#endif
}

void zest__add_work_queue_entry(zest_context context, void *data, zest_work_queue_callback call_back) {
    if (!context->device->thread_count) {
        call_back(&context->device->work_queue, data);
        return;
    }

    zest_uint new_entry_to_write = (context->device->work_queue.next_write_entry + 1) % ZEST_MAX_QUEUE_ENTRIES;
    while (new_entry_to_write == context->device->work_queue.next_read_entry) {        //Not enough room in work queue
        //We can do this because we're single producer
        zest__do_next_work_queue_entry(&context->device->work_queue);
    }
    context->device->work_queue.entries[context->device->work_queue.next_write_entry].data = data;
    context->device->work_queue.entries[context->device->work_queue.next_write_entry].call_back = call_back;
    zest__atomic_increment(&context->device->work_queue.entry_completion_goal);

    zest__writebarrier;

    zest__push_queue_work(&context->device->thread_queues, &context->device->work_queue);
    context->device->work_queue.next_write_entry = new_entry_to_write;

}

// --Buffer & Memory Management
void *zest_AllocateMemory(zest_context context, zest_size size) {
    return ZEST__ALLOCATE(context->device->allocator, size);
}

void zest_FreeMemory(zest_context context, void *allocation) {
    ZEST__FREE(context->device->allocator, allocation);
}

void* zest__allocate_aligned(zloc_allocator *allocator, zest_size size, zest_size alignment) {
    void* allocation = zloc_AllocateAligned(allocator, size, alignment);
    if (!allocation) {
		zest_device device = (zest_device)allocator->user_data;
        zest__add_host_memory_pool(device, size);
        allocation = zloc_AllocateAligned(device->allocator, size, alignment);
        ZEST_ASSERT(allocation);    //Unable to allocate even after adding a pool
    }
    return allocation;
}

void zest__unmap_memory(zest_device_memory_pool memory_allocation) {
    if (memory_allocation->mapped) {
		zest_context context = memory_allocation->context;
		context->device->platform->unmap_memory(memory_allocation);
        memory_allocation->mapped = ZEST_NULL;
    }
}

void zest__destroy_memory(zest_device_memory_pool memory_allocation) {
	zest_context context = memory_allocation->context;
    context->device->platform->cleanup_memory_pool_backend(memory_allocation);
    ZEST__FREE(memory_allocation->context->device->allocator, memory_allocation);
    memory_allocation->mapped = ZEST_NULL;
}

void zest__on_add_pool(void* user_data, void* block) {
    zest_buffer_allocator_t* pools = (zest_buffer_allocator_t*)user_data;
    zest_buffer_t* buffer = (zest_buffer_t*)block;
    buffer->size = pools->memory_pools[zest_vec_last_index(pools->memory_pools)]->size;
    buffer->memory_pool = pools->memory_pools[zest_vec_last_index(pools->memory_pools)];
    buffer->memory_offset = 0;
}

void zest__on_split_block(void* user_data, zloc_header* block, zloc_header* trimmed_block, zest_size remote_size) {
    zest_buffer_allocator_t* pools = (zest_buffer_allocator_t*)user_data;
    zest_buffer buffer = (zest_buffer)zloc_BlockUserExtensionPtr(block);
    zest_buffer trimmed_buffer = (zest_buffer)zloc_BlockUserExtensionPtr(trimmed_block);
    trimmed_buffer->size = buffer->size - remote_size;
    buffer->size = remote_size;
    trimmed_buffer->memory_offset = buffer->memory_offset + buffer->size;
    //--
    trimmed_buffer->memory_pool = buffer->memory_pool;
    trimmed_buffer->buffer_allocator = buffer->buffer_allocator;
    trimmed_buffer->memory_in_use = 0;
    if (pools->buffer_info.property_flags & zest_memory_property_host_visible_bit) {
        buffer->data = (void*)((char*)buffer->memory_pool->mapped + buffer->memory_offset);
        buffer->end = (void*)((char*)buffer->data + buffer->size);
    }
}

void zest__on_reallocation_copy(void* user_data, zloc_header* block, zloc_header* new_block) {
    zest_buffer_allocator pools = (zest_buffer_allocator_t*)user_data;
    zest_buffer buffer = (zest_buffer)zloc_BlockUserExtensionPtr(block);
    zest_buffer new_buffer = (zest_buffer)zloc_BlockUserExtensionPtr(new_block);
    if (pools->buffer_info.property_flags & zest_memory_property_host_visible_bit) {
        new_buffer->data = (void*)((char*)new_buffer->memory_pool->mapped + new_buffer->memory_offset);
        new_buffer->end = (void*)((char*)new_buffer->data + new_buffer->size);
        memcpy(new_buffer->data, buffer->data, buffer->size);
    }
}

void zest__remote_merge_next_callback(void *user_data, zloc_header *block, zloc_header *next_block) {
    zest_buffer_allocator buffer_allocator = (zest_buffer_allocator)user_data;
	ZEST_ASSERT_HANDLE(buffer_allocator);
    zest_buffer remote_block = (zest_buffer)zloc_BlockUserExtensionPtr(block);
    zest_buffer next_remote_block = (zest_buffer)zloc_BlockUserExtensionPtr(next_block);
    remote_block->size += next_remote_block->size;
    next_remote_block->magic = 0;
    next_remote_block->memory_offset = 0;
    next_remote_block->size = 0;
	zest_context context = buffer_allocator->context;
	context->device->platform->push_buffer_for_freeing(remote_block);
	context->device->platform->push_buffer_for_freeing(next_remote_block);
    remote_block->magic = 0;
}

void zest__remote_merge_prev_callback(void *user_data, zloc_header *prev_block, zloc_header *block) {
    zest_buffer_allocator buffer_allocator = (zest_buffer_allocator)user_data;
	ZEST_ASSERT_HANDLE(buffer_allocator);
    zest_buffer remote_block = (zest_buffer)zloc_BlockUserExtensionPtr(block);
    zest_buffer prev_remote_block = (zest_buffer)zloc_BlockUserExtensionPtr(prev_block);
    prev_remote_block->size += remote_block->size;
    prev_remote_block->magic = 0;
    remote_block->memory_offset = 0;
    remote_block->size = 0;
	zest_context context = buffer_allocator->context;
	context->device->platform->push_buffer_for_freeing(remote_block);
	context->device->platform->push_buffer_for_freeing(prev_remote_block);
    remote_block->magic = 0;
}

zloc_size zest__get_minimum_block_size(zest_size pool_size) {
    return pool_size > zloc__MEGABYTE(1) ? pool_size / 128 : 256;
}

zest_bool zest__create_memory_pool(zest_context context, zest_buffer_info_t *buffer_info, zest_key key, zest_size minimum_size, zest_device_memory_pool *memory_pool) {
    zest_device_memory_pool buffer_pool = (zest_device_memory_pool)ZEST__NEW(context->device->allocator, zest_device_memory_pool);
    *buffer_pool = ZEST__ZERO_INIT(zest_device_memory_pool_t);
    buffer_pool->magic = zest_INIT_MAGIC(zest_struct_type_device_memory_pool);
	buffer_pool->context = context;
    buffer_pool->backend = (zest_device_memory_pool_backend)context->device->platform->new_memory_pool_backend(context);
    buffer_pool->flags = buffer_info->flags;
	zest_buffer_pool_size_t pre_defined_pool_size = zest_GetDeviceBufferPoolSize(context, buffer_info->buffer_usage_flags, buffer_info->property_flags, buffer_info->image_usage_flags);
    zest_bool result = ZEST_TRUE;
    if (pre_defined_pool_size.pool_size > 0) {
        buffer_pool->name = pre_defined_pool_size.name;
        buffer_pool->size = pre_defined_pool_size.pool_size > minimum_size ? pre_defined_pool_size.pool_size : zest_GetNextPower(minimum_size + minimum_size / 2);
        buffer_pool->minimum_allocation_size = pre_defined_pool_size.minimum_allocation_size;
    } else {
        if (buffer_info->image_usage_flags) {
            ZEST_PRINT_WARNING(ZEST_WARNING_COLOR"Allocating image memory where no default pool size was found for image usage flags: %i, and property flags: %i. Defaulting to next power from size + size / 2",
                buffer_info->image_usage_flags, buffer_info->property_flags);
        } else {
            ZEST_PRINT_WARNING(ZEST_WARNING_COLOR"Allocating memory where no default pool size was found for usage flags: %i and property flags: %i. Defaulting to next power from size + size / 2",
                buffer_info->buffer_usage_flags, buffer_info->property_flags);
        }
        //Todo: we need a better solution then this
        buffer_pool->size = ZEST__MAX(zest_GetNextPower(minimum_size + minimum_size / 2), zloc__MEGABYTE(16));
        buffer_pool->name = "Unknown";
        buffer_pool->minimum_allocation_size = zest__get_minimum_block_size(buffer_pool->size);
    }
    if (buffer_info->buffer_usage_flags) {
        result = context->device->platform->create_buffer_memory_pool(context, buffer_pool->size, buffer_info, buffer_pool, "");
        if (result != ZEST_TRUE) {
            ZEST_APPEND_LOG(context->device->log_path.str, "Unable to allocate memory for buffer memory pool. Tried to allocate %zu.", buffer_pool->size);
            goto cleanup;
            return result;
        }
    }
    else {
        result = context->device->platform->create_image_memory_pool(context, buffer_pool->size, buffer_info, buffer_pool);
        if (result != ZEST_TRUE) {
            ZEST_APPEND_LOG(context->device->log_path.str, "Unable to allocate memory for image memory pool. Tried to allocate %zu.", buffer_pool->size);
            goto cleanup;
            return result;
        }
    }
    if (buffer_info->property_flags & zest_memory_property_host_visible_bit) {
        context->device->platform->map_memory(buffer_pool, ZEST_WHOLE_SIZE, 0);
    }
    *memory_pool = buffer_pool;
    return ZEST_TRUE;
    cleanup:
    zest__destroy_memory(buffer_pool);
    return ZEST_FALSE;
}

void zest__add_remote_range_pool(zest_buffer_allocator buffer_allocator, zest_device_memory_pool buffer_pool) {
    zest_vec_push(buffer_pool->context->device->allocator, buffer_allocator->memory_pools, buffer_pool);
    zloc_size range_pool_size = zloc_CalculateRemoteBlockPoolSize(buffer_allocator->allocator, buffer_pool->size);
    zest_pool_range* range_pool = (zest_pool_range*)ZEST__ALLOCATE(buffer_pool->context->device->allocator, range_pool_size);
    zest_vec_push(buffer_pool->context->device->allocator, buffer_allocator->range_pools, range_pool);
    zloc_AddRemotePool(buffer_allocator->allocator, range_pool, range_pool_size, buffer_pool->size);
    ZEST_PRINT_NOTICE(ZEST_NOTICE_COLOR"Note: Ran out of space in the Device memory pool (%s) so adding a new one of size %zu. ", buffer_pool->name, (size_t)buffer_pool->size);
}

void zest__set_buffer_details(zest_context context, zest_buffer_allocator buffer_allocator, zest_buffer buffer, zest_bool is_host_visible) {
	buffer->context = context;
    buffer->backend = (zest_buffer_backend)context->device->platform->new_buffer_backend(buffer->context);
	context->device->platform->set_buffer_backend_details(buffer);
    buffer->magic = zest_INIT_MAGIC(zest_struct_type_buffer);
    buffer->buffer_allocator = buffer_allocator;
    buffer->memory_in_use = 0;
    buffer->array_index = ZEST_INVALID;
    buffer->owner_queue_family = ZEST_QUEUE_FAMILY_IGNORED;
    if (is_host_visible) {
        buffer->data = (void*)((char*)buffer->memory_pool->mapped + buffer->memory_offset);
        buffer->end = (void*)((char*)buffer->data + buffer->size);
    }
    else {
        buffer->data = ZEST_NULL;
        buffer->end = ZEST_NULL;
    }
}

void zest_FlushUsedBuffers(zest_context context) {
	context->device->platform->flush_used_buffers(context);
}

void zest__cleanup_buffers_in_allocators(zest_context context) {
    zest_map_foreach(i, context->device->buffer_allocators) {
        zest_buffer_allocator allocator = context->device->buffer_allocators.data[i];
        zest_vec_foreach(j, allocator->range_pools) {
            zest_pool_range pool = allocator->range_pools[j];
            zloc_header *current_block = (zloc_header*)zloc__first_block_in_pool((zloc_pool*)pool);
            while (!zloc__is_last_block_in_pool(current_block)) {
                zest_buffer remote_block = (zest_buffer)zloc_BlockUserExtensionPtr(current_block);
                if (ZEST_IS_INTITIALISED(remote_block->magic)) {
                    context->device->platform->cleanup_buffer_backend(remote_block);
                }
                zloc_header *last_block = current_block;
                current_block = zloc__next_physical_block(current_block);
            }
        }
    }
}

void zloc__output_buffer_info(void* ptr, size_t size, int free, void* user, int count)
{
    zest_buffer_t* buffer = (zest_buffer_t*)user;
    printf("%i) \t%s range size: \t%zi \tbuffer size: %llu \toffset: %llu \n", count, free ? "free" : "used", size, buffer->size, buffer->buffer_offset);
}

zloc__error_codes zloc_VerifyAllRemoteBlocks(zest_context context, zloc__block_output output_function, void* user_data) {
    zest_map_foreach(i, context->device->buffer_allocators) {
        zest_buffer_allocator allocator = context->device->buffer_allocators.data[i];
        zest_vec_foreach(j, allocator->range_pools) {
            zest_pool_range pool = allocator->range_pools[j];
            zloc_header *current_block = (zloc_header*)zloc__first_block_in_pool((zloc_pool*)pool);
            int count = 0;
            while (!zloc__is_last_block_in_pool(current_block)) {
                void *remote_block = zloc_BlockUserExtensionPtr(current_block);
                if (output_function) {
                    output_function(current_block, zloc__block_size(current_block), zloc__is_free_block(current_block), remote_block, ++count);
                }
                zloc_header *last_block = current_block;
                current_block = zloc__next_physical_block(current_block);
                if (last_block != current_block->prev_physical_block) {
                    ZEST_ASSERT(0);
                    return zloc__PHYSICAL_BLOCK_MISALIGNMENT;
                }
            }
        }
    }
    return zloc__OK;
}

zloc__error_codes zloc_VerifyRemoteBlocks(zloc_header* first_block, zloc__block_output output_function, void* user_data) {
    zloc_header* current_block = first_block;
    int count = 0;
    while (!zloc__is_last_block_in_pool(current_block)) {
        void* remote_block = zloc_BlockUserExtensionPtr(current_block);
        if (output_function) {
            output_function(current_block, zloc__block_size(current_block), zloc__is_free_block(current_block), remote_block, ++count);
        }
        zloc_header* last_block = current_block;
        current_block = zloc__next_physical_block(current_block);
        if (last_block != current_block->prev_physical_block) {
            ZEST_ASSERT(0);
            return zloc__PHYSICAL_BLOCK_MISALIGNMENT;
        }
    }
    return zloc__OK;
}

zloc__error_codes zloc_VerifyBlocks(zloc_header* first_block, zloc__block_output output_function, void* user_data) {
    zloc_header* current_block = first_block;
    while (!zloc__is_last_block_in_pool(current_block)) {
        if (output_function) {
            output_function(current_block, zloc__block_size(current_block), zloc__is_free_block(current_block), user_data, 0);
        }
        zloc_header* last_block = current_block;
        current_block = zloc__next_physical_block(current_block);
        if (last_block != current_block->prev_physical_block) {
            ZEST_ASSERT(0);
            return zloc__PHYSICAL_BLOCK_MISALIGNMENT;
        }
    }
    return zloc__OK;
}

zest_uint zloc_CountBlocks(zloc_header* first_block) {
    zloc_header* current_block = first_block;
    int count = 0;
    while (!zloc__is_last_block_in_pool(current_block)) {
        void* remote_block = zloc_BlockUserExtensionPtr(current_block);
        zloc_header* last_block = current_block;
        current_block = zloc__next_physical_block(current_block);
        ZEST_ASSERT(last_block == current_block->prev_physical_block);
        count++;
    }
    return count;
}

zest_buffer zest_CreateBuffer(zest_context context, zest_size size, zest_buffer_info_t* buffer_info) {
    zest_key key = zest_map_hash_ptr(context->device->buffer_allocators, buffer_info, sizeof(zest_buffer_info_t));
    if (!zest_map_valid_key(context->device->buffer_allocators, key)) {
        //If an allocator doesn't exist yet for this combination of usage and buffer properties then create one.

        zest_buffer_allocator buffer_allocator = (zest_buffer_allocator)ZEST__NEW(context->device->allocator, zest_buffer_allocator);
        *buffer_allocator = ZEST__ZERO_INIT(zest_buffer_allocator_t);
        buffer_allocator->buffer_info = *buffer_info;
        buffer_allocator->magic = zest_INIT_MAGIC(zest_struct_type_buffer_allocator);
		buffer_allocator->context = context;
        zest_device_memory_pool buffer_pool = 0;
        if (zest__create_memory_pool(context, buffer_info, key, size, &buffer_pool) != ZEST_TRUE) {
			return 0;
        }

        buffer_allocator->alignment = buffer_pool->alignment;
        zest_vec_push(context->device->allocator, buffer_allocator->memory_pools, buffer_pool);
        buffer_allocator->allocator = (zloc_allocator*)ZEST__ALLOCATE(context->device->allocator, zloc_AllocatorSize());
        buffer_allocator->allocator = zloc_InitialiseAllocatorForRemote(buffer_allocator->allocator);
        zloc_SetBlockExtensionSize(buffer_allocator->allocator, sizeof(zest_buffer_t));
        zloc_SetMinimumAllocationSize(buffer_allocator->allocator, buffer_pool->minimum_allocation_size);
        zloc_size range_pool_size = zloc_CalculateRemoteBlockPoolSize(buffer_allocator->allocator, buffer_pool->size);
        zest_pool_range* range_pool = (zest_pool_range*)ZEST__ALLOCATE(context->device->allocator, range_pool_size);
        zest_vec_push(context->device->allocator, buffer_allocator->range_pools, range_pool);
        zest_map_insert_key(context->device->allocator, context->device->buffer_allocators, key, buffer_allocator);
        buffer_allocator->allocator->remote_user_data = *zest_map_at_key(context->device->buffer_allocators, key);
        buffer_allocator->allocator->add_pool_callback = zest__on_add_pool;
        buffer_allocator->allocator->split_block_callback = zest__on_split_block;
        buffer_allocator->allocator->unable_to_reallocate_callback = zest__on_reallocation_copy;
        buffer_allocator->allocator->merge_next_callback = zest__remote_merge_next_callback;
        buffer_allocator->allocator->merge_prev_callback = zest__remote_merge_prev_callback;
        zloc_AddRemotePool(buffer_allocator->allocator, range_pool, range_pool_size, buffer_pool->size);
    }

    zest_buffer_allocator buffer_allocator = *zest_map_at_key(context->device->buffer_allocators, key);
    zloc_size adjusted_size = zloc__align_size_up(size, buffer_allocator->alignment);
    zest_buffer buffer = (zest_buffer)zloc_AllocateRemote(buffer_allocator->allocator, adjusted_size);
    if (buffer) {
        zest__set_buffer_details(context, buffer_allocator, buffer, buffer_info->property_flags & zest_memory_property_host_visible_bit);
    }
    else {
        zest_device_memory_pool buffer_pool = 0;
        if(zest__create_memory_pool(context, buffer_info, key, size, &buffer_pool) != ZEST_TRUE) {
            return 0;
        }
        ZEST_ASSERT(buffer_pool->alignment == buffer_allocator->alignment);    //The new pool should have the same alignment as the alignment set in the allocator, this
        //would have been set when the first pool was created

        zest__add_remote_range_pool(buffer_allocator, buffer_pool);
        buffer = (zest_buffer)zloc_AllocateRemote(buffer_allocator->allocator, adjusted_size);
        if (!buffer) {    //Unable to allocate memory. Out of memory?
            ZEST_APPEND_LOG(context->device->log_path.str, "Unable to allocate %zu of memory.", size);
            return 0;
        }
        zest__set_buffer_details(context, buffer_allocator, buffer, buffer_info->property_flags & zest_memory_property_host_visible_bit);
    }

	buffer->context = context;
    return buffer;
}

zest_buffer zest_CreateStagingBuffer(zest_context context, zest_size size, void* data) {
    zest_buffer_info_t buffer_info = zest_CreateBufferInfo(zest_buffer_type_staging, zest_memory_usage_cpu_to_gpu);
	buffer_info.frame_in_flight = context->current_fif;
    zest_buffer buffer = zest_CreateBuffer(context, size, &buffer_info);
    if (data) {
        memcpy(buffer->data, data, size);
    }
    return buffer;
}

void zest_BeginImmediateCommandBuffer(zest_context context) {
	ZEST_ASSERT_HANDLE(context);		//Not a valid context handle
    context->device->platform->begin_single_time_commands(context);
}

void zest_EndImmediateCommandBuffer(zest_context context) {
	ZEST_ASSERT_HANDLE(context);		//Not a valid context handle
    context->device->platform->end_single_time_commands(context);
}

zest_bool zest_imm_CopyBuffer(zest_context context, zest_buffer src_buffer, zest_buffer dst_buffer, zest_size size) {
	ZEST_ASSERT_HANDLE(context);		//Not a valid context handle
    ZEST_ASSERT(size <= src_buffer->size);        //size must be less than or equal to the staging buffer size and the device buffer size
    ZEST_ASSERT(size <= dst_buffer->size);
	context->device->platform->cmd_copy_buffer_one_time(src_buffer, dst_buffer, size);
    return ZEST_TRUE;
}

void zest_StageData(void *src_data, zest_buffer dst_staging_buffer, zest_size size) {
    ZEST_ASSERT(dst_staging_buffer->data);  //No mapped data in staging buffer, is it an actual staging buffer?
    ZEST_ASSERT(src_data);                  //No source data to copy!
    ZEST_ASSERT(size <= dst_staging_buffer->size);  //Staging buffer not large enough
    memcpy(dst_staging_buffer->data, src_data, size);
    dst_staging_buffer->memory_in_use = size;
}

zest_bool zest_GrowBuffer(zest_buffer* buffer, zest_size unit_size, zest_size minimum_bytes) {
    ZEST_ASSERT(unit_size);
    if (minimum_bytes && (*buffer)->size > minimum_bytes) {
        return ZEST_FALSE;
    }
    zest_size units = (*buffer)->size / unit_size;
    zest_size new_size = (units ? units + units / 2 : 8) * unit_size;
    new_size = ZEST__MAX(new_size, minimum_bytes);
    if (new_size <= (*buffer)->size) {
        return ZEST_FALSE;
    }
	zest_context context = (*buffer)->context;
    zest_buffer_allocator_t* buffer_allocator = (*buffer)->buffer_allocator;
    zest_size memory_in_use = (*buffer)->memory_in_use;
    zest_buffer new_buffer = (zest_buffer)zloc_ReallocateRemote(buffer_allocator->allocator, *buffer, new_size);
	//Preserve the bindless array index
	zest_uint array_index = (*buffer)->array_index;
    if (new_buffer) {
        new_buffer->buffer_allocator = (*buffer)->buffer_allocator;
		context->device->platform->cleanup_buffer_backend(*buffer);
        *buffer = new_buffer;
        zest__set_buffer_details(context, buffer_allocator, *buffer, buffer_allocator->buffer_info.property_flags & zest_memory_property_host_visible_bit);
        new_buffer->array_index = array_index;
        new_buffer->memory_in_use = memory_in_use;
    }
    else {
        //Create a new memory pool and try again
        zest_device_memory_pool buffer_pool = 0;
        if (zest__create_memory_pool(context, &buffer_allocator->buffer_info, 0, new_size, &buffer_pool) != ZEST_TRUE) {
            return ZEST_FALSE;
        } else {
            ZEST_ASSERT(buffer_pool->alignment == buffer_allocator->alignment);    //The new pool should have the same alignment as the alignment set in the allocator, this
            //would have been set when the first pool was created
            zest__add_remote_range_pool(buffer_allocator, buffer_pool);
            new_buffer = (zest_buffer)zloc_ReallocateRemote(buffer_allocator->allocator, *buffer, new_size);
            ZEST_ASSERT(new_buffer);    //Unable to allocate memory. Out of memory?
			context->device->platform->cleanup_buffer_backend(*buffer);
            *buffer = new_buffer;
            zest__set_buffer_details(context, buffer_allocator, *buffer, buffer_allocator->buffer_info.property_flags & zest_memory_property_host_visible_bit);
            new_buffer->array_index = array_index;
            new_buffer->memory_in_use = memory_in_use;
        }
    }
    return new_buffer ? ZEST_TRUE : ZEST_FALSE;
}

zest_bool zest_ResizeBuffer(zest_buffer *buffer, zest_size new_size) {
    ZEST_ASSERT(new_size);
    if ((*buffer)->size > new_size) {
        return ZEST_FALSE;
    }
	zest_context context = (*buffer)->context;
    zest_buffer_allocator_t* buffer_allocator = (*buffer)->buffer_allocator;
    zest_size memory_in_use = (*buffer)->memory_in_use;
    zest_buffer new_buffer = (zest_buffer)zloc_ReallocateRemote(buffer_allocator->allocator, *buffer, new_size);
    if (new_buffer) {
        new_buffer->buffer_allocator = (*buffer)->buffer_allocator;
		context->device->platform->cleanup_buffer_backend(*buffer);
        *buffer = new_buffer;
        zest__set_buffer_details(context, buffer_allocator, *buffer, buffer_allocator->buffer_info.property_flags & zest_memory_property_host_visible_bit);
        new_buffer->memory_in_use = memory_in_use;
    }
    else {
        //Create a new memory pool and try again
        zest_device_memory_pool buffer_pool = 0;
        if (zest__create_memory_pool(context, &buffer_allocator->buffer_info, 0, new_size, &buffer_pool) != ZEST_TRUE) {
            return ZEST_FALSE;
        } else {
            ZEST_ASSERT(buffer_pool->alignment == buffer_allocator->alignment);    //The new pool should have the same alignment as the alignment set in the allocator, this
            //would have been set when the first pool was created
            zest__add_remote_range_pool(buffer_allocator, buffer_pool);
            new_buffer = (zest_buffer)zloc_ReallocateRemote(buffer_allocator->allocator, *buffer, new_size);
            ZEST_ASSERT(new_buffer);    //Unable to allocate memory. Out of memory?
			context->device->platform->cleanup_buffer_backend(*buffer);
            *buffer = new_buffer;
            zest__set_buffer_details(context, buffer_allocator, *buffer, buffer_allocator->buffer_info.property_flags & zest_memory_property_host_visible_bit);
            new_buffer->memory_in_use = memory_in_use;
        }
    }
    return new_buffer ? ZEST_TRUE : ZEST_FALSE;
}

zest_buffer_info_t zest_CreateBufferInfo(zest_buffer_type type, zest_memory_usage usage) {
    zest_buffer_info_t buffer_info = ZEST__ZERO_INIT(zest_buffer_info_t);
	//Implicit src and dst flags
    buffer_info.buffer_usage_flags = zest_buffer_usage_transfer_dst_bit | zest_buffer_usage_transfer_src_bit;
	switch (usage) {
		case zest_memory_usage_gpu_only: buffer_info.property_flags = zest_memory_property_device_local_bit; break;
		case zest_memory_usage_cpu_to_gpu: buffer_info.property_flags = zest_memory_property_host_visible_bit | zest_memory_property_host_coherent_bit; break;
		case zest_memory_usage_gpu_to_cpu: buffer_info.property_flags = zest_memory_property_host_visible_bit | zest_memory_property_host_cached_bit; break;
		default: break;
	}
	switch (type) {
		case zest_buffer_type_staging: buffer_info.flags = zest_memory_pool_flag_single_buffer; break;
		case zest_buffer_type_vertex: buffer_info.buffer_usage_flags |= zest_buffer_usage_vertex_buffer_bit; break;
		case zest_buffer_type_index: buffer_info.buffer_usage_flags |= zest_buffer_usage_index_buffer_bit; break;
		case zest_buffer_type_uniform: buffer_info.buffer_usage_flags |= zest_buffer_usage_uniform_buffer_bit; break;
		case zest_buffer_type_storage: buffer_info.buffer_usage_flags |= zest_buffer_usage_storage_buffer_bit; break;
		case zest_buffer_type_indirect: buffer_info.buffer_usage_flags |= zest_buffer_usage_indirect_buffer_bit; break;
		case zest_buffer_type_vertex_storage: buffer_info.buffer_usage_flags |= zest_buffer_usage_storage_buffer_bit | zest_buffer_usage_vertex_buffer_bit; break;
		case zest_buffer_type_index_storage: buffer_info.buffer_usage_flags |= zest_buffer_usage_storage_buffer_bit | zest_buffer_usage_index_buffer_bit; break;
		default: break;
	}
    
	return buffer_info;
}

void zest_FreeBuffer(zest_buffer buffer) {
    if (!buffer) return;    		//Nothing to free
	if (buffer->size == 0) return;	//Buffer was already freed;
    zloc_FreeRemote(buffer->buffer_allocator->allocator, buffer);
	zest_context context = buffer->context;
    context->device->platform->cleanup_buffer_backend(buffer);
}

void zest_AddCopyCommand(zest_buffer_uploader_t *uploader, zest_buffer source_buffer, zest_buffer target_buffer, zest_size target_offset) {
    if (uploader->flags & zest_buffer_upload_flag_initialised) {
        ZEST_ASSERT(uploader->source_buffer == source_buffer && uploader->target_buffer == target_buffer);    //Buffer uploads must be to the same source and target ids with each copy. Use a separate BufferUpload for each combination of source and target buffers
    }

    uploader->source_buffer = source_buffer;
    uploader->target_buffer = target_buffer;
    uploader->flags |= zest_buffer_upload_flag_initialised;

	zest_context context = source_buffer->context;
    zest_buffer_copy_t buffer_info = ZEST__ZERO_INIT(zest_buffer_copy_t);
    buffer_info.src_offset = source_buffer->buffer_offset;
    buffer_info.dst_offset = target_offset;
    ZEST_ASSERT(source_buffer->memory_in_use <= target_buffer->size + target_offset);
    buffer_info.size = source_buffer->memory_in_use;
    zest_vec_linear_push(context->frame_graph_allocator[context->current_fif], uploader->buffer_copies, buffer_info);
    target_buffer->memory_in_use = source_buffer->memory_in_use;
}

zest_buffer_pool_size_t zest_GetDevicePoolSize(zest_context context, zest_key hash) {
    if (zest_map_valid_key(context->device->pool_sizes, hash)) {
        return *zest_map_at_key(context->device->pool_sizes, hash);
    }
    zest_buffer_pool_size_t pool_size = ZEST__ZERO_INIT(zest_buffer_pool_size_t);
    return pool_size;
}

zest_buffer_pool_size_t zest_GetDeviceBufferPoolSize(zest_context context, zest_buffer_usage_flags buffer_usage_flags, zest_memory_property_flags property_flags, zest_image_usage_flags image_flags) {
    zest_buffer_usage_t usage;
    usage.buffer_usage_flags = buffer_usage_flags;
    usage.property_flags = property_flags;
    usage.image_flags = image_flags;
    zest_key usage_hash = zest_Hash(&usage, sizeof(zest_buffer_usage_t), ZEST_HASH_SEED);
    if (zest_map_valid_key(context->device->pool_sizes, usage_hash)) {
        return *zest_map_at_key(context->device->pool_sizes, usage_hash);
    }
    zest_buffer_pool_size_t pool_size = ZEST__ZERO_INIT(zest_buffer_pool_size_t);
    return pool_size;
}

zest_buffer_pool_size_t zest_GetDeviceImagePoolSize(zest_context context, const char* name) {
    zest_key usage_hash = zest_Hash(name, strlen(name), ZEST_HASH_SEED);
    if (zest_map_valid_key(context->device->pool_sizes, usage_hash)) {
        return *zest_map_at_key(context->device->pool_sizes, usage_hash);
    }
    zest_buffer_pool_size_t pool_size = ZEST__ZERO_INIT(zest_buffer_pool_size_t);
    return pool_size;
}

void zest_SetDeviceBufferPoolSize(zest_device device, const char *name, zest_buffer_usage_flags buffer_usage_flags, zest_memory_property_flags property_flags, zest_size minimum_allocation, zest_size pool_size) {
    ZEST_ASSERT(pool_size);                    //Must set a pool size
    ZEST_ASSERT(ZEST__POW2(pool_size));        //Pool size must be a power of 2
    zest_index size_index = ZEST__MAX(zloc__scan_forward(pool_size) - 20, 0);
    minimum_allocation = ZEST__MAX(minimum_allocation, 64 << size_index);
    zest_buffer_usage_t usage = ZEST__ZERO_INIT(zest_buffer_usage_t);
    usage.buffer_usage_flags = buffer_usage_flags;
    usage.property_flags = property_flags;
    zest_key usage_hash = zest_Hash(&usage, sizeof(zest_buffer_usage_t), ZEST_HASH_SEED);
    zest_buffer_pool_size_t pool_sizes;
    pool_sizes.name = name;
    pool_sizes.minimum_allocation_size = minimum_allocation;
    pool_sizes.pool_size = pool_size;
    zest_map_insert_key(device->allocator, device->pool_sizes, usage_hash, pool_sizes);
}

void zest_SetDeviceImagePoolSize(zest_device device, const char *name, zest_image_usage_flags image_flags, zest_memory_property_flags property_flags, zest_size minimum_allocation, zest_size pool_size) {
    ZEST_ASSERT(pool_size);                    //Must set a pool size
    ZEST_ASSERT(ZEST__POW2(pool_size));        //Pool size must be a power of 2
    zest_index size_index = ZEST__MAX(zloc__scan_forward(pool_size) - 20, 0);
    minimum_allocation = ZEST__MAX(minimum_allocation, 64 << size_index);
    zest_buffer_usage_t usage = ZEST__ZERO_INIT(zest_buffer_usage_t);
    usage.image_flags = image_flags;
    usage.property_flags = property_flags;
    zest_key usage_hash = zest_Hash(&usage, sizeof(zest_buffer_usage_t), ZEST_HASH_SEED);
    zest_buffer_pool_size_t pool_sizes;
    pool_sizes.name = name;
    pool_sizes.minimum_allocation_size = minimum_allocation;
    pool_sizes.pool_size = pool_size;
    zest_map_insert_key(device->allocator, device->pool_sizes, usage_hash, pool_sizes);
}
// --End Vulkan Buffer Management

// --Renderer and related functions
zest_bool zest__initialise_context(zest_context context, zest_create_info_t* create_info) {
    context->flags |= (create_info->flags & zest_init_flag_enable_vsync) ? zest_context_flag_vsync_enabled : 0;
    zest_SetText(context->device->allocator, &context->device->shader_path_prefix, create_info->shader_path_prefix);
    ZEST_APPEND_LOG(context->device->log_path.str, "Create swap chain");

	void *store_memory = ZEST__ALLOCATE(context->device->allocator, create_info->store_allocator_size);
	context->store_allocator = zloc_InitialiseAllocatorWithPool(store_memory, create_info->store_allocator_size);

	for (int i = 0; i != zest_max_handle_type; ++i) {
		switch ((zest_handle_type)i) {
			case zest_handle_type_shader_resources: zest__initialise_store(context, &context->resource_stores[i], sizeof(zest_shader_resources_t)); break;
			case zest_handle_type_images: zest__initialise_store(context, &context->resource_stores[i], sizeof(zest_image_t)); break;
			case zest_handle_type_views: zest__initialise_store(context, &context->resource_stores[i], sizeof(zest_image_view)); break;
			case zest_handle_type_view_arrays: zest__initialise_store(context, &context->resource_stores[i], sizeof(zest_image_view_array)); break;
			case zest_handle_type_samplers: zest__initialise_store(context, &context->resource_stores[i], sizeof(zest_sampler_t)); break;
			case zest_handle_type_uniform_buffers: zest__initialise_store(context, &context->resource_stores[i], sizeof(zest_uniform_buffer_t)); break;
			case zest_handle_type_timers: zest__initialise_store(context, &context->resource_stores[i], sizeof(zest_timer_t)); break;
			case zest_handle_type_layers: zest__initialise_store(context, &context->resource_stores[i], sizeof(zest_layer_t)); break;
			case zest_handle_type_shaders: zest__initialise_store(context, &context->resource_stores[i], sizeof(zest_shader_t)); break;
			case zest_handle_type_compute_pipelines: zest__initialise_store(context, &context->resource_stores[i], sizeof(zest_compute_t)); break;
			case zest_handle_type_set_layouts: zest__initialise_store(context, &context->resource_stores[i], sizeof(zest_set_layout_t)); break;
			case zest_handle_type_image_collection: zest__initialise_store(context, &context->resource_stores[i], sizeof(zest_image_collection_t)); break;
		}
	}

	if (!context->device->platform->create_window_surface(context)) {
		ZEST_APPEND_LOG(context->device->log_path.str, "Unable to create window surface");
		ZEST_PRINT("Unable to create window surface");
		return ZEST_FALSE;
	}
    context->swapchain = zest__create_swapchain(context, create_info->title);

    ZEST_APPEND_LOG(context->device->log_path.str, "Create descriptor layouts");

    //Create a global bindless descriptor set for storage buffers and texture samplers
    zest_set_layout_builder_t layout_builder = zest_BeginSetLayoutBuilder(context);
    zest_AddLayoutBuilderBinding(&layout_builder, ZEST_STRUCT_LITERAL( zest_descriptor_binding_desc_t, zest_sampler_binding, zest_descriptor_type_sampler, create_info->bindless_sampler_count, zest_shader_compute_stage | zest_shader_fragment_stage ) );
    zest_AddLayoutBuilderBinding(&layout_builder, ZEST_STRUCT_LITERAL( zest_descriptor_binding_desc_t, zest_texture_2d_binding, zest_descriptor_type_sampled_image, create_info->bindless_texture_2d_count, zest_shader_compute_stage | zest_shader_fragment_stage ) );
    zest_AddLayoutBuilderBinding(&layout_builder, ZEST_STRUCT_LITERAL( zest_descriptor_binding_desc_t, zest_texture_cube_binding, zest_descriptor_type_sampled_image, create_info->bindless_texture_cube_count, zest_shader_compute_stage | zest_shader_fragment_stage ) );
    zest_AddLayoutBuilderBinding(&layout_builder, ZEST_STRUCT_LITERAL( zest_descriptor_binding_desc_t, zest_texture_array_binding, zest_descriptor_type_sampled_image, create_info->bindless_texture_array_count, zest_shader_compute_stage | zest_shader_fragment_stage ) );
    zest_AddLayoutBuilderBinding(&layout_builder, ZEST_STRUCT_LITERAL( zest_descriptor_binding_desc_t, zest_texture_3d_binding, zest_descriptor_type_sampled_image, create_info->bindless_texture_3d_count, zest_shader_compute_stage | zest_shader_fragment_stage ) );
    zest_AddLayoutBuilderBinding(&layout_builder, ZEST_STRUCT_LITERAL( zest_descriptor_binding_desc_t, zest_storage_buffer_binding, zest_descriptor_type_storage_buffer, create_info->bindless_storage_buffer_count, zest_shader_all_stages ) );
    zest_AddLayoutBuilderBinding(&layout_builder, ZEST_STRUCT_LITERAL( zest_descriptor_binding_desc_t, zest_storage_image_binding, zest_descriptor_type_storage_image, create_info->bindless_storage_image_count, zest_shader_compute_stage | zest_shader_fragment_stage ) );
    context->device->global_bindless_set_layout_handle = zest_FinishDescriptorSetLayoutForBindless(&layout_builder, 1, "Zest Descriptor Layout");
    context->device->global_bindless_set_layout = (zest_set_layout)zest__get_store_resource(context->device->global_bindless_set_layout_handle.context, context->device->global_bindless_set_layout_handle.value);
    context->device->global_set = zest_CreateBindlessSet(context->device->global_bindless_set_layout_handle);

    context->device->platform->set_depth_format(context);

    ZEST_APPEND_LOG(context->device->log_path.str, "Compile shaders");
    zest__compile_builtin_shaders(context);
    ZEST_APPEND_LOG(context->device->log_path.str, "Create standard pipelines");
    zest__prepare_standard_pipelines(context);

    if (!context->device->platform->initialise_context_backend(context)) {
        return ZEST_FALSE;
    }

    zest_ForEachFrameInFlight(fif) {
		void *frame_graph_linear_memory = ZEST__ALLOCATE(context->device->allocator, zloc__MEGABYTE(1));
        context->frame_graph_allocator[fif] = zloc_InitialiseLinearAllocator(frame_graph_linear_memory, zloc__MEGABYTE(1));
		ZEST_ASSERT(context->frame_graph_allocator[fif]);    //Unabable to allocate the frame graph allocator, 
    }

    ZEST_APPEND_LOG(context->device->log_path.str, "Finished zest initialisation");

    ZEST__FLAG(context->flags, zest_context_flag_initialised);

    return ZEST_TRUE;
}

zest_swapchain zest__create_swapchain(zest_context context, const char *name) {
    zest_swapchain swapchain = (zest_swapchain)ZEST__NEW(context->device->allocator, zest_swapchain);
    *swapchain = ZEST__ZERO_INIT(zest_swapchain_t);
    swapchain->magic = zest_INIT_MAGIC(zest_struct_type_swapchain);
    swapchain->backend = (zest_swapchain_backend)context->device->platform->new_swapchain_backend(context);
	swapchain->context = context;
    swapchain->name = name;
	context->swapchain = swapchain;
    if (!context->device->platform->initialise_swapchain(context)) {
        zest__cleanup_swapchain(swapchain);
        return NULL;
    }

    swapchain->resolution.x = 1.f / swapchain->size.width;
    swapchain->resolution.y = 1.f / swapchain->size.height;
    return swapchain;
}

void zest__cleanup_swapchain(zest_swapchain swapchain) {
	zest_context context = swapchain->context;
    context->device->platform->cleanup_swapchain_backend(swapchain);
	context->device->platform->destroy_context_surface(context);
    zest_vec_free(context->device->allocator, swapchain->images);
    zest_vec_free(context->device->allocator, swapchain->views);
	ZEST__FREE(context->device->allocator, swapchain);
}

void zest__cleanup_pipeline(zest_pipeline pipeline) {
	zest_context context = pipeline->context;
	context->device->platform->cleanup_pipeline_backend(pipeline);
	ZEST__FREE(context->device->allocator, pipeline);
}

void zest__cleanup_pipelines(zest_context context) {
    zest_map_foreach(i, context->device->cached_pipelines) {
        zest_pipeline pipeline = *zest_map_at_index(context->device->cached_pipelines, i);
        context->device->platform->cleanup_pipeline_backend(pipeline);
        ZEST__FREE(context->device->allocator, pipeline);
    }
}

void zest__cleanup_device(zest_device device) {
	zest_vec_foreach(i, device->queues) {
		zest_queue queue = device->queues[i];
		device->platform->cleanup_queue_backend(device, queue);
	}
	device->platform->cleanup_device_backend(device);
    ZEST__FREE(device->allocator, device->scratch_arena);
    zest_vec_free(device->allocator, device->extensions);
    zest_vec_free(device->allocator, device->queues);
    zest_map_free(device->allocator, device->queue_names);
    zest_map_free(device->allocator, device->pool_sizes);
    zest_FreeText(device->allocator, &device->log_path);
}

void zest__free_handle(void *handle) {
    zest_struct_type struct_type = (zest_struct_type)ZEST_STRUCT_TYPE(handle);
    switch (struct_type) {
    case zest_struct_type_pipeline_template:
        zest__cleanup_pipeline_template((zest_pipeline_template)handle);
        break;
    case zest_struct_type_image:
        zest__cleanup_image((zest_image)handle);
        break;
    }

}

void zest__scan_memory_and_free_resources(zest_context context) {
    zloc_pool_stats_t stats = zloc_CreateMemorySnapshot(zloc__first_block_in_pool(zloc_GetPool(context->device->allocator)));
    if (stats.used_blocks == 0) {
        return;
    }
    void **memory_to_free = 0;
    zest_vec_reserve(context->device->allocator, memory_to_free, (zest_uint)stats.used_blocks);
    zloc_header *current_block = zloc__first_block_in_pool(zloc_GetPool(context->device->allocator));
    while (!zloc__is_last_block_in_pool(current_block)) {
        if (!zloc__is_free_block(current_block)) {
            zest_size block_size = zloc__block_size(current_block);
            void *allocation = (void *)((char *)current_block + zloc__BLOCK_POINTER_OFFSET);
            if (ZEST_VALID_HANDLE(allocation)) {
                zest_struct_type struct_type = (zest_struct_type)ZEST_STRUCT_TYPE(allocation);
                switch (struct_type) {
                case zest_struct_type_pipeline_template:
                    zest_vec_push(context->device->allocator, memory_to_free, allocation);
                    break;
                }
            }
        }
        current_block = zloc__next_physical_block(current_block);
    }
    zest_vec_foreach(i, memory_to_free) {
        void *allocation = memory_to_free[i];
        zest__free_handle(allocation);
    }
    zest_vec_free(context->device->allocator, memory_to_free);
}

void zest__cleanup_image_store(zest_context context) {
	zest_resource_store_t *store = &context->resource_stores[zest_handle_type_images];
    zest_image_t *images = (zest_image_t*)store->data;
    for (int i = 0; i != store->current_size; ++i) {
        if (ZEST_VALID_HANDLE(&images[i])) {
            zest_image resource = &images[i];
            zest__cleanup_image(resource);
        }
    }
	zest__free_store(store);
}

void zest__cleanup_sampler_store(zest_context context) {
	zest_resource_store_t *store = &context->resource_stores[zest_handle_type_samplers];
    zest_sampler_t *samplers = (zest_sampler_t*)store->data;
    for (int i = 0; i != store->current_size; ++i) {
        if (ZEST_VALID_HANDLE(&samplers[i])) {
            zest_sampler resource = &samplers[i];
            zest__cleanup_sampler(resource);
        }
    }
	zest__free_store(store);
}

void zest__cleanup_uniform_buffer_store(zest_context context) {
	zest_resource_store_t *store = &context->resource_stores[zest_handle_type_uniform_buffers];
    zest_uniform_buffer_t *uniform_buffers = (zest_uniform_buffer_t*)store->data;
    for (int i = 0; i != store->current_size; ++i) {
        if (ZEST_VALID_HANDLE(&uniform_buffers[i])) {
            zest_uniform_buffer resource = &uniform_buffers[i];
            zest__cleanup_uniform_buffer(resource);
        }
    }
	zest__free_store(store);
}

void zest__cleanup_timer_store(zest_context context) {
	zest_resource_store_t *store = &context->resource_stores[zest_handle_type_timers];
    zest_timer_t *timers = (zest_timer_t*)store->data;
    for (int i = 0; i != store->current_size; ++i) {
        if (ZEST_VALID_HANDLE(&timers[i])) {
            zest_timer resource = &timers[i];
            zest_FreeTimer(resource->handle);
        }
    }
	zest__free_store(store);
}

void zest__cleanup_layer_store(zest_context context) {
	zest_resource_store_t *store = &context->resource_stores[zest_handle_type_layers];
    zest_layer_t *layers = (zest_layer_t*)store->data;
    for (int i = 0; i != store->current_size; ++i) {
        if (ZEST_VALID_HANDLE(&layers[i])) {
            zest_layer resource = &layers[i];
            zest__cleanup_layer(resource);
        }
    }
	zest__free_store(store);
}

void zest__cleanup_shader_store(zest_context context) {
	zest_resource_store_t *store = &context->resource_stores[zest_handle_type_shaders];
    zest_shader_t *shaders = (zest_shader_t*)store->data;
    for (int i = 0; i != store->current_size; ++i) {
        if (ZEST_VALID_HANDLE(&shaders[i])) {
            zest_shader resource = &shaders[i];
            zest_FreeShader(resource->handle);
        }
    }
	zest__free_store(store);
}

void zest__cleanup_compute_store(zest_context context) {
	zest_resource_store_t *store = &context->resource_stores[zest_handle_type_compute_pipelines];
    zest_compute_t *compute_pipelines = (zest_compute_t*)store->data;
    for (int i = 0; i != store->current_size; ++i) {
        if (ZEST_VALID_HANDLE(&compute_pipelines[i])) {
            zest_compute resource = &compute_pipelines[i];
            zest__cleanup_compute(resource);
        }
    }
	zest__free_store(store);
}

void zest__cleanup_set_layout_store(zest_context context) {
	zest_resource_store_t *store = &context->resource_stores[zest_handle_type_set_layouts];
    zest_set_layout_t *set_layouts = (zest_set_layout_t*)store->data;
    for (int i = 0; i != store->current_size; ++i) {
        if (ZEST_VALID_HANDLE(&set_layouts[i])) {
            zest_set_layout resource = &set_layouts[i];
            zest__cleanup_set_layout(resource);
        }
    }
	zest__free_store(store);
}

void zest__cleanup_shader_resource_store(zest_context context) {
	zest_resource_store_t *store = &context->resource_stores[zest_handle_type_shader_resources];
    zest_shader_resources_t *shader_resources = (zest_shader_resources_t*)store->data;
    for (int i = 0; i != store->current_size; ++i) {
        if (ZEST_VALID_HANDLE(&shader_resources[i])) {
            zest_shader_resources resource = &shader_resources[i];
            context->device->platform->cleanup_shader_resources_backend(resource);
        }
    }
	zest__free_store(store);
}

void zest__cleanup_view_store(zest_context context) {
	zest_resource_store_t *store = &context->resource_stores[zest_handle_type_views];
    zest_image_view *views = (zest_image_view*)store->data;
    for (int i = 0; i != store->current_size; ++i) {
        if (ZEST_VALID_HANDLE(views[i])) {
            zest_image_view resource = views[i];
            zest__cleanup_image_view(resource);
        }
    }
	zest__free_store(store);
}

void zest__cleanup_view_array_store(zest_context context) {
	zest_resource_store_t *store = &context->resource_stores[zest_handle_type_view_arrays];
    zest_image_view_array *view_arrays = (zest_image_view_array*)store->data;
    for (int i = 0; i != store->current_size; ++i) {
        if (ZEST_VALID_HANDLE(view_arrays[i])) {
            zest_image_view_array resource = view_arrays[i];
            zest__cleanup_image_view_array(resource);
        }
    }
	zest__free_store(store);
}

void zest__cleanup_image_collection_store(zest_context context) {
	zest_resource_store_t *store = &context->resource_stores[zest_handle_type_image_collection];
    zest_image_collection_t *image_collections = (zest_image_collection_t*)store->data;
    for (int i = 0; i != store->current_size; ++i) {
        if (ZEST_VALID_HANDLE(&image_collections[i])) {
            zest_image_collection resource = &image_collections[i];
            zest__cleanup_image_collection(resource);
        }
    }
	zest__free_store(store);
}

void zest__cleanup_context(zest_context context) {
    zest_uint cached_pipelines_size = zest_map_size(context->device->cached_pipelines);

    zest__cleanup_shader_resource_store(context);
    zest__cleanup_image_store(context);
    zest__cleanup_uniform_buffer_store(context);
    zest__cleanup_timer_store(context);
    zest__cleanup_layer_store(context);
    zest__cleanup_shader_store(context);
    zest__cleanup_compute_store(context);
    zest__cleanup_set_layout_store(context);
    zest__cleanup_sampler_store(context);
    zest__cleanup_view_store(context);
    zest__cleanup_view_array_store(context);
    zest__cleanup_image_collection_store(context);

	ZEST__FREE(context->device->allocator, context->store_allocator);

    context->device->global_bindless_set_layout_handle = ZEST__ZERO_INIT(zest_set_layout_handle);
    context->device->global_bindless_set_layout = 0;

	zest__cleanup_swapchain(context->swapchain);

    zest__scan_memory_and_free_resources(context);

    zest__cleanup_pipelines(context);

    zest_map_foreach(i, context->device->cached_frame_graph_semaphores) {
        zest_frame_graph_semaphores semaphores = context->device->cached_frame_graph_semaphores.data[i];
        context->device->platform->cleanup_frame_graph_semaphore(context, semaphores);
        ZEST__FREE(context->device->allocator, semaphores);
    }

    zest_FlushUsedBuffers(context);
    zest__cleanup_buffers_in_allocators(context);

    zest_ForEachFrameInFlight(fif) {
        if (zest_vec_size(context->device->deferred_resource_freeing_list.images[fif])) {
            zest_vec_foreach(i, context->device->deferred_resource_freeing_list.images[fif]) {
                zest_image image = &context->device->deferred_resource_freeing_list.images[fif][i];
                context->device->platform->cleanup_image_backend(image);
                zest_FreeBuffer(image->buffer);
                if (image->default_view) {
                    context->device->platform->cleanup_image_view_backend(image->default_view);
                }
            }
            zest_vec_clear(context->device->deferred_resource_freeing_list.images[fif]);
        }

        if (zest_vec_size(context->device->deferred_resource_freeing_list.views[fif])) {
            zest_vec_foreach(i, context->device->deferred_resource_freeing_list.views[fif]) {
                zest_image_view view = &context->device->deferred_resource_freeing_list.views[fif][i];
                context->device->platform->cleanup_image_view_backend(view);
            }
            zest_vec_clear(context->device->deferred_resource_freeing_list.views[fif]);
        }

		if (zest_vec_size(context->device->deferred_resource_freeing_list.view_arrays[fif])) {
			zest_vec_foreach(i, context->device->deferred_resource_freeing_list.view_arrays[fif]) {
				zest_image_view_array view_array = &context->device->deferred_resource_freeing_list.view_arrays[fif][i];
				context->device->platform->cleanup_image_view_array_backend(view_array);
			}
			zest_vec_clear(context->device->deferred_resource_freeing_list.view_arrays[fif]);
		}
    }

    zest_map_foreach(i, context->device->buffer_allocators) {
        zest_buffer_allocator buffer_allocator = *zest_map_at_index(context->device->buffer_allocators, i);
        zest_vec_foreach(j, buffer_allocator->memory_pools) {
            zest__destroy_memory(buffer_allocator->memory_pools[j]);
        }
        zest_vec_free(context->device->allocator, buffer_allocator->memory_pools);
        zest_vec_foreach(j, buffer_allocator->range_pools) {
            ZEST__FREE(context->device->allocator, buffer_allocator->range_pools[j]);
        }
        zest_vec_free(context->device->allocator, buffer_allocator->range_pools);
        ZEST__FREE(context->device->allocator, buffer_allocator->allocator);
        ZEST__FREE(context->device->allocator, buffer_allocator);
    }

    zest_map_foreach(i, context->device->reports) {
        zest_report_t *report = &context->device->reports.data[i];
        zest_FreeText(context->device->allocator, &report->message);
    }

    zest_map_foreach(i, context->device->cached_frame_graphs) {
        zest_cached_frame_graph_t *cached_graph = &context->device->cached_frame_graphs.data[i];
        ZEST__FREE(context->device->allocator, cached_graph->memory);
    }

    zest_ForEachFrameInFlight(fif) {
        ZEST__FREE(context->device->allocator, context->frame_graph_allocator[fif]);
        zest_vec_free(context->device->allocator, context->device->deferred_resource_freeing_list.resources[fif]);
        zest_vec_free(context->device->allocator, context->device->deferred_resource_freeing_list.images[fif]);
        zest_vec_free(context->device->allocator, context->device->deferred_resource_freeing_list.binding_indexes[fif]);
        zest_vec_free(context->device->allocator, context->device->deferred_resource_freeing_list.views[fif]);
        zest_vec_free(context->device->allocator, context->device->deferred_resource_freeing_list.view_arrays[fif]);
    }

    ZEST__FREE(context->device->allocator, context->device->global_set->backend);
    ZEST__FREE(context->device->allocator, context->device->global_set);

    zest_vec_free(context->device->allocator, context->device->deferred_staging_buffers_for_freeing);
    zest_vec_free(context->device->allocator, context->device->debug.frame_log);
    zest_map_free(context->device->allocator, context->device->reports);

    zest_map_free(context->device->allocator, context->device->buffer_allocators);
    zest_map_free(context->device->allocator, context->device->cached_pipelines);
    zest_map_free(context->device->allocator, context->device->cached_frame_graph_semaphores);
    zest_map_free(context->device->allocator, context->device->cached_frame_graphs);

    zest_FreeText(context->device->allocator, &context->device->shader_path_prefix);

	context->device->platform->cleanup_context_backend(context);
}

zest_bool zest__recreate_swapchain(zest_context context) {
	zest_swapchain swapchain = context->swapchain;
    int fb_width = 0, fb_height = 0;
    int window_width = 0, window_height = 0;
    while (fb_width == 0 || fb_height == 0) {
        context->window_data.window_sizes_callback(context->window_data.window_handle, &fb_width, &fb_height, &window_width, &window_height);
    }

    swapchain->size = ZEST_STRUCT_LITERAL(zest_extent2d_t, (zest_uint)fb_width, (zest_uint)fb_height );
    swapchain->resolution.x = 1.f / fb_width;
    swapchain->resolution.y = 1.f / fb_height;

    zest_WaitForIdleDevice(context);

	const char *name = swapchain->name;

	//clean up the swapchain except for the surface, that can be re-used
    context->device->platform->cleanup_swapchain_backend(swapchain);
    zest_vec_free(context->device->allocator, swapchain->images);
    zest_vec_free(context->device->allocator, swapchain->views);
	ZEST__FREE(context->device->allocator, swapchain);

    zest__cleanup_pipelines(context);
    zest_map_free(context->device->allocator, context->device->cached_pipelines);

	swapchain = zest__create_swapchain(context, name);
    ZEST__FLAG(swapchain->flags, zest_swapchain_flag_was_recreated);
    return swapchain != NULL;
}

zest_uniform_buffer_handle zest_CreateUniformBuffer(zest_context context, const char *name, zest_size uniform_struct_size) {
    zest_uniform_buffer_handle handle = { zest__add_store_resource(zest_handle_type_uniform_buffers, context) };
    zest_uniform_buffer uniform_buffer = (zest_uniform_buffer)zest__get_store_resource_checked(context, handle.value);
	handle.context = context;
    *uniform_buffer = ZEST__ZERO_INIT(zest_uniform_buffer_t);
    uniform_buffer->magic = zest_INIT_MAGIC(zest_struct_type_uniform_buffer);
    uniform_buffer->handle = handle;
    zest_buffer_info_t buffer_info = ZEST__ZERO_INIT(zest_buffer_info_t);
    buffer_info.buffer_usage_flags = zest_buffer_usage_uniform_buffer_bit;
    buffer_info.property_flags = zest_memory_property_host_visible_bit | zest_memory_property_host_coherent_bit;

	zest_set_layout_builder_t uniform_layout_builder = zest_BeginSetLayoutBuilder(context);
    zest_AddLayoutBuilderBinding(&uniform_layout_builder, ZEST_STRUCT_LITERAL(zest_descriptor_binding_desc_t, 0, zest_descriptor_type_uniform_buffer, 1, zest_shader_all_stages ) );
	uniform_buffer->set_layout = zest_FinishDescriptorSetLayout(&uniform_layout_builder, "Layout for: %s", name);
	zest_CreateDescriptorPoolForLayout(uniform_buffer->set_layout, ZEST_MAX_FIF);

    zest_set_layout set_layout = (zest_set_layout)zest__get_store_resource_checked(context, uniform_buffer->set_layout.value);
    zest_ForEachFrameInFlight(fif) {
        uniform_buffer->buffer[fif] = zest_CreateBuffer(context, uniform_struct_size, &buffer_info);
    }
    uniform_buffer->backend = (zest_uniform_buffer_backend)context->device->platform->new_uniform_buffer_backend(context);
    context->device->platform->set_uniform_buffer_backend(uniform_buffer);
    context->device->platform->create_uniform_descriptor_set(uniform_buffer, set_layout);
    return handle;
}

void zest_FreeUniformBuffer(zest_uniform_buffer_handle handle) {
	zest_context context = handle.context;
    zest_uniform_buffer uniform_buffer = (zest_uniform_buffer)zest__get_store_resource_checked(context, handle.value);
    zest_vec_push(context->device->allocator, context->device->deferred_resource_freeing_list.resources[context->current_fif], uniform_buffer);
}

zest_set_layout_builder_t zest_BeginSetLayoutBuilder(zest_context context) {
    zest_set_layout_builder_t builder = ZEST__ZERO_INIT(zest_set_layout_builder_t);
	builder.context = context;
    return builder;
}

bool zest__binding_exists_in_layout_builder(zest_set_layout_builder_t *builder, zest_uint binding) {
    zest_vec_foreach(i, builder->bindings) {
        if (builder->bindings[i].binding == binding) {
            return true;
        }
    }
    return false;
}

void zest_AddLayoutBuilderBinding( zest_set_layout_builder_t *builder, zest_descriptor_binding_desc_t description) {
    bool binding_exists = zest__binding_exists_in_layout_builder(builder, description.binding);
    ZEST_ASSERT(!binding_exists);       //That binding number already exists in the layout builder
    zest_vec_push(builder->context->device->allocator, builder->bindings, description);
}

zest_set_layout_handle zest_FinishDescriptorSetLayout(zest_set_layout_builder_t *builder, const char *name, ...) {
	zest_context context = builder->context;
    ZEST_ASSERT(name);  //Give the descriptor set a unique name
    zest_text_t layout_name = ZEST__ZERO_INIT(zest_text_t);
    va_list args;
    va_start(args, name);
    zest_SetTextfv(context->device->allocator, &layout_name, name, args);
    va_end(args);
    ZEST_ASSERT(builder->bindings);     //must have bindings to create the layout
    zest_uint binding_count = (zest_uint)zest_vec_size(builder->bindings);
    ZEST_ASSERT(binding_count > 0);     //Must add bindings before finishing the descriptor layout builder

    zest_set_layout_handle handle = ZEST__ZERO_INIT(zest_set_layout_handle);
    handle = zest__new_descriptor_set_layout(builder->context, layout_name.str);
    zest_set_layout set_layout = (zest_set_layout)zest__get_store_resource_checked(builder->context, handle.value);

    if (!context->device->platform->create_set_layout(builder, set_layout, ZEST_FALSE)) {
		zest_vec_free(builder->context->device->allocator, builder->bindings);
        zest__cleanup_set_layout(set_layout);
		zest_FreeText(builder->context->device->allocator, &layout_name);
        return ZEST__ZERO_INIT(zest_set_layout_handle);
    }

    zest_vec_resize(builder->context->device->allocator, set_layout->descriptor_indexes, zest_vec_size(builder->bindings));
    zest_vec_foreach(i, builder->bindings) {
        set_layout->descriptor_indexes[i] = ZEST__ZERO_INIT(zest_descriptor_indices_t);
        set_layout->descriptor_indexes[i].capacity = builder->bindings[i].count;
        set_layout->descriptor_indexes[i].descriptor_type = builder->bindings[i].type;
    }

    set_layout->bindings = builder->bindings;
    zest_FreeText(builder->context->device->allocator, &layout_name);
    return handle;
}

zest_set_layout_handle zest_FinishDescriptorSetLayoutForBindless(zest_set_layout_builder_t *builder, zest_uint num_global_sets_this_pool_should_support, const char *name, ...) {
	zest_context context = builder->context;
    ZEST_ASSERT(name);  //Give the descriptor set a unique name
    zest_text_t layout_name = ZEST__ZERO_INIT(zest_text_t);
    va_list args;
    va_start(args, name);
    zest_SetTextfv(context->device->allocator, &layout_name, name, args);
    va_end(args);
    ZEST_ASSERT(builder->bindings);     //must have bindings to create the layout
    zest_uint binding_count = (zest_uint)zest_vec_size(builder->bindings);
	ZEST_ASSERT(binding_count > 0);     //Must add bindings before finishing the descriptor layout builder

    zest_set_layout_handle handle = zest__new_descriptor_set_layout(builder->context, layout_name.str);
    zest_set_layout set_layout = (zest_set_layout)zest__get_store_resource_checked(builder->context, handle.value);
    if (!context->device->platform->create_set_layout(builder, set_layout, ZEST_TRUE)) {
		zest_vec_free(context->device->allocator, builder->bindings);
        zest__cleanup_set_layout(set_layout);
		zest_FreeText(context->device->allocator, &layout_name);
        return ZEST__ZERO_INIT(zest_set_layout_handle);
    }

    ZEST__FLAG(set_layout->flags, zest_set_layout_flag_bindless);

    zest_CreateDescriptorPoolForLayout(handle, 1);

    zest_vec_resize(context->device->allocator, set_layout->descriptor_indexes, zest_vec_size(builder->bindings));
    zest_vec_foreach(i, builder->bindings) {
        set_layout->descriptor_indexes[i] = ZEST__ZERO_INIT(zest_descriptor_indices_t);
        set_layout->descriptor_indexes[i].capacity = builder->bindings[i].count;
        set_layout->descriptor_indexes[i].descriptor_type = builder->bindings[i].type;
    }

    set_layout->bindings = builder->bindings;
    zest_FreeText(context->device->allocator, &layout_name);
    return handle;
}

ZEST_API zest_descriptor_set zest_CreateBindlessSet(zest_set_layout_handle layout_handle) {
    zest_set_layout layout = (zest_set_layout)zest__get_store_resource_checked(layout_handle.context, layout_handle.value);
    // max_sets_in_pool was set during pool creation, usually 1 for a global bindless set.
    ZEST_ASSERT(layout->pool->max_sets >= 1 && "Pool was not created to allow allocation of at least one set.");

	zest_context context = layout_handle.context;
    return context->device->platform->create_bindless_set(layout);
}

zest_set_layout_handle zest__new_descriptor_set_layout(zest_context context, const char *name) {
    zest_set_layout_handle handle = ZEST_STRUCT_LITERAL(zest_set_layout_handle, zest__add_store_resource(zest_handle_type_set_layouts, context) );
    zest_set_layout descriptor_layout = (zest_set_layout)zest__get_store_resource(context, handle.value);
    *descriptor_layout = ZEST__ZERO_INIT(zest_set_layout_t);
    descriptor_layout->backend = (zest_set_layout_backend)context->device->platform->new_set_layout_backend(context);
    descriptor_layout->name.str = 0;
    descriptor_layout->magic = zest_INIT_MAGIC(zest_struct_type_set_layout);
	handle.context = context;
    descriptor_layout->handle = handle;
    zest_SetText(context->device->allocator, &descriptor_layout->name, name);
    return handle;
}


zest_uint zest__acquire_bindless_index(zest_set_layout layout, zest_uint binding_number) {
    if (binding_number >= zest_vec_size(layout->descriptor_indexes)) {
        ZEST_PRINT("Attempted to acquire index for out-of-bounds binding_number %u for layout '%s'. Are you sure this is a layout that's configured for bindless descriptors?", binding_number, layout->name.str);
        return ZEST_INVALID; 
    }

    zest_descriptor_indices_t *manager = &layout->descriptor_indexes[binding_number];

    if (zest_vec_size(manager->free_indices) > 0) {
        zest_uint index = zest_vec_back(manager->free_indices); // Reuse from free list
        zest_vec_pop(manager->free_indices);
        return index;
    } else {
        if (manager->next_new_index < manager->capacity) {
            return manager->next_new_index++; // Allocate a new one sequentially
        } else {
            ZEST_PRINT("Ran out of bindless indices for binding %u (type %d, capacity %u) in layout '%s'!",
                binding_number, manager->descriptor_type, manager->capacity, layout->name.str);
            return ZEST_INVALID;
        }
    }
}

void zest__release_bindless_index(zest_set_layout layout, zest_uint binding_number, zest_uint index_to_release) {
    ZEST_ASSERT_HANDLE(layout);   //Not a valid layout handle!
    if (index_to_release == ZEST_INVALID) return;

    if (binding_number >= zest_vec_size(layout->descriptor_indexes)) {
        ZEST_PRINT("Attempted to release index for out-of-bounds binding_number %u for layout '%s'. Check that layout is actually intended for bindless.", binding_number, layout->name.str);
        return;
    }

    zest_descriptor_indices_t *manager = &layout->descriptor_indexes[binding_number];

    zest_vec_foreach(i, manager->free_indices) {
        if (index_to_release == manager->free_indices[i]) {
			ZEST_PRINT("Attempted to release index for binding_number %u for layout '%s' that is already free. Make sure the binding number is correct.", binding_number, layout->name.str);
            return;
        }
    }

    ZEST_ASSERT(index_to_release < manager->capacity);
    zest_vec_push(layout->handle.context->device->allocator, manager->free_indices, index_to_release);
}

void zest__cleanup_set_layout(zest_set_layout layout) {
	zest_context context = layout->handle.context;
    zest_FreeText(context->device->allocator, &layout->name);
	zest_vec_foreach(i, layout->descriptor_indexes) {
		zest_vec_free(context->device->allocator, layout->descriptor_indexes[i].free_indices);
	}
	zest_vec_free(context->device->allocator, layout->descriptor_indexes);
    context->device->platform->cleanup_set_layout_backend(layout);
    ZEST__FREE(context->device->allocator, layout->pool);
    zest_vec_free(context->device->allocator, layout->bindings);
    zest__remove_store_resource(context, layout->handle.value);
}

void zest__cleanup_image_view(zest_image_view view) {
	zest_context context = view->handle.context;
    context->device->platform->cleanup_image_view_backend(view);
    if (view->handle.value) {
        //Default views in images are not in resource storage
        zest__remove_store_resource(context, view->handle.value);
    }
    ZEST__FREE(context->device->allocator, view);
}

void zest__cleanup_image_view_array(zest_image_view_array view_array) {
	zest_context context = view_array->handle.context;
    context->device->platform->cleanup_image_view_array_backend(view_array);
    ZEST__FREE(context->device->allocator, view_array->views);
    zest__remove_store_resource(context, view_array->handle.value);
    ZEST__FREE(context->device->allocator, view_array);
}

void zest__add_descriptor_set_to_resources(zest_context context, zest_shader_resources resources, zest_descriptor_set descriptor_set, zest_uint fif) {
	zest_vec_push(context->device->allocator, resources->sets[fif], descriptor_set);
}

zest_shader_resources_handle zest_CreateShaderResources(zest_context context) {
    zest_shader_resources_handle handle = { zest__add_store_resource(zest_handle_type_shader_resources, context) };
    zest_shader_resources bundle = (zest_shader_resources)zest__get_store_resource_checked(context, handle.value);
    *bundle = ZEST__ZERO_INIT(zest_shader_resources_t);
    bundle->magic = zest_INIT_MAGIC(zest_struct_type_shader_resources);
	handle.context = context;
	bundle->handle = handle;
    return handle;
}

void zest_FreeShaderResources(zest_shader_resources_handle handle) {
    zest_shader_resources shader_resources = (zest_shader_resources)zest__get_store_resource_checked(handle.context, handle.value);
    if (ZEST_VALID_HANDLE(shader_resources)) {
        zest_ForEachFrameInFlight(fif) {
            zest_vec_free(handle.context->device->allocator, shader_resources->sets[fif]);
        }
		handle.context->device->platform->cleanup_shader_resources_backend(shader_resources);
        zest__remove_store_resource(handle.context, handle.value);
    }
}

void zest_AddDescriptorSetToResources(zest_shader_resources_handle handle, zest_descriptor_set descriptor_set, zest_uint fif) {
	zest_context context = handle.context;
    zest_shader_resources resources = (zest_shader_resources)zest__get_store_resource_checked(handle.context, handle.value);
    ZEST_ASSERT_HANDLE(resources);   //Not a valid shader resource handle
	zest_vec_push(handle.context->device->allocator, resources->sets[fif], descriptor_set);
}

void zest_AddUniformBufferToResources(zest_shader_resources_handle shader_resources_handle, zest_uniform_buffer_handle buffer_handle) {
	ZEST_ASSERT(shader_resources_handle.context == buffer_handle.context);	//Both handles must be within the same context.
    zest_shader_resources shader_resources = (zest_shader_resources)zest__get_store_resource_checked(shader_resources_handle.context, shader_resources_handle.value);
    zest_uniform_buffer uniform_buffer = (zest_uniform_buffer)zest__get_store_resource_checked(buffer_handle.context, buffer_handle.value);
    zest_ForEachFrameInFlight(fif) {
        zest__add_descriptor_set_to_resources(shader_resources_handle.context, shader_resources, uniform_buffer->descriptor_set[fif], fif);
    }
}

void zest_AddGlobalBindlessSetToResources(zest_shader_resources_handle handle) {
    zest_shader_resources shader_resources = (zest_shader_resources)zest__get_store_resource_checked(handle.context, handle.value);
	zest_context context = handle.context;
    zest_ForEachFrameInFlight(fif) {
        zest__add_descriptor_set_to_resources(handle.context, shader_resources, context->device->global_set, fif);
    }
}

void zest_AddBindlessSetToResources(zest_shader_resources_handle handle, zest_descriptor_set set) {
    zest_shader_resources shader_resources = (zest_shader_resources)zest__get_store_resource_checked(handle.context, handle.value);
    zest_ForEachFrameInFlight(fif) {
        zest__add_descriptor_set_to_resources(handle.context, shader_resources, set, fif);
    }
}

void zest_UpdateShaderResources(zest_shader_resources_handle handle, zest_descriptor_set descriptor_set, zest_uint index, zest_uint fif) {
    zest_shader_resources shader_resources = (zest_shader_resources)zest__get_store_resource_checked(handle.context, handle.value);
	ZEST_ASSERT(index < zest_vec_size(shader_resources->sets));    //Must be a valid index for the descriptor set in the resources that you want to update.
	shader_resources->sets[fif][index] = descriptor_set;
}

void zest_ClearShaderResources(zest_shader_resources_handle handle) {
    zest_shader_resources shader_resources = (zest_shader_resources)zest__get_store_resource_checked(handle.context, handle.value);
    if (!shader_resources) return;
    zest_vec_clear(shader_resources->sets);
}

bool zest_ValidateShaderResource(zest_shader_resources_handle handle) {
    zest_shader_resources shader_resources = (zest_shader_resources)zest__get_store_resource_checked(handle.context, handle.value);
    zest_ForEachFrameInFlight(fif) {
        zest_uint set_size = zest_vec_size(shader_resources->sets[fif]);
        zest_vec_foreach(i, shader_resources->sets[fif]) {
            if (!ZEST_VALID_HANDLE(shader_resources->sets[fif][i])) {
                ZEST_PRINT("Descriptor set at frame in flight %i index %i was not valid. You need to make sure that you add a descriptor set for all frames in flight in the shader resource.", fif, i);
                return false;
            }
        }
    }
    return true;
}

zest_descriptor_pool zest__create_descriptor_pool(zest_context context, zest_uint max_sets) {
    zest_descriptor_pool_t blank = ZEST__ZERO_INIT(zest_descriptor_pool_t);
    zest_descriptor_pool pool = (zest_descriptor_pool)ZEST__NEW(context->device->allocator, zest_descriptor_pool);
    *pool = blank;
    pool->max_sets = max_sets;
    pool->magic = zest_INIT_MAGIC(zest_struct_type_descriptor_pool);
    pool->backend = (zest_descriptor_pool_backend)context->device->platform->new_descriptor_pool_backend(context);
    return pool;
}

zest_bool zest_CreateDescriptorPoolForLayout(zest_set_layout_handle layout_handle, zest_uint max_set_count) {
    zest_set_layout layout = (zest_set_layout)zest__get_store_resource_checked(layout_handle.context, layout_handle.value);

	zest_bool bindless = ZEST__FLAGGED(layout->flags, zest_set_layout_flag_bindless);
	max_set_count = bindless ? 1 : max_set_count;
	zest_descriptor_pool pool = zest__create_descriptor_pool(layout_handle.context, max_set_count);
	zest_context context = layout_handle.context;
	if (!context->device->platform->create_set_pool(pool, layout, max_set_count, bindless)) {
		ZEST__FREE(layout_handle.context->device->allocator, pool);
	}
	layout->pool = pool;
	return ZEST_TRUE;
}

zest_pipeline zest__create_pipeline(zest_context context) {
    zest_pipeline pipeline = (zest_pipeline)ZEST__NEW(context->device->allocator, zest_pipeline);
    *pipeline = ZEST__ZERO_INIT(zest_pipeline_t);
    pipeline->magic = zest_INIT_MAGIC(zest_struct_type_pipeline);
    pipeline->backend = (zest_pipeline_backend)context->device->platform->new_pipeline_backend(context);
	pipeline->context = context;
    return pipeline;
}

void zest_SetPipelineVertShader(zest_pipeline_template pipeline_template, zest_shader_handle shader_handle) {
    ZEST_ASSERT_HANDLE(pipeline_template);
    zest_shader vertex_shader = (zest_shader)zest__get_store_resource_checked(shader_handle.context, shader_handle.value);
    ZEST_ASSERT_HANDLE(vertex_shader);  //Not a valid vertex shader handle
    pipeline_template->vertex_shader = shader_handle;
}

void zest_SetPipelineFragShader(zest_pipeline_template pipeline_template, zest_shader_handle shader_handle) {
    ZEST_ASSERT_HANDLE(pipeline_template);
    zest_shader fragment_shader = (zest_shader)zest__get_store_resource_checked(shader_handle.context, shader_handle.value);
    ZEST_ASSERT_HANDLE(fragment_shader);	//Not a valid fragment shader handle
    pipeline_template->fragment_shader = shader_handle;
}

void zest_SetPipelineShaders(zest_pipeline_template pipeline_template, zest_shader_handle vertex_shader_handle, zest_shader_handle fragment_shader_handle) {
    ZEST_ASSERT_HANDLE(pipeline_template);
	ZEST_ASSERT(vertex_shader_handle.context == fragment_shader_handle.context);	//Vertex and fragment shader must be in the same context!
    zest_shader vertex_shader = (zest_shader)zest__get_store_resource_checked(vertex_shader_handle.context, vertex_shader_handle.value);
    zest_shader fragment_shader = (zest_shader)zest__get_store_resource_checked(fragment_shader_handle.context, fragment_shader_handle.value);
    ZEST_ASSERT_HANDLE(vertex_shader);       //Not a valid vertex shader handle
    ZEST_ASSERT_HANDLE(fragment_shader);     //Not a valid fragment shader handle
    pipeline_template->vertex_shader = vertex_shader_handle;
    pipeline_template->fragment_shader = fragment_shader_handle;
}

void zest_SetPipelineShader(zest_pipeline_template pipeline_template, zest_shader_handle combined_vertex_and_fragment_shader_handle) {
    ZEST_ASSERT_HANDLE(pipeline_template);
    zest_shader combined_vertex_and_fragment_shader = (zest_shader)zest__get_store_resource_checked(combined_vertex_and_fragment_shader_handle.context, combined_vertex_and_fragment_shader_handle.value);
    ZEST_ASSERT_HANDLE(combined_vertex_and_fragment_shader);       //Not a valid shader handle
    pipeline_template->vertex_shader = combined_vertex_and_fragment_shader_handle;
}

void zest_SetPipelineFrontFace(zest_pipeline_template pipeline_template, zest_front_face front_face) {
    ZEST_ASSERT_HANDLE(pipeline_template);  //invalid pipeline template handle
    pipeline_template->rasterization.front_face = front_face;
}

void zest_SetPipelineTopology(zest_pipeline_template pipeline_template, zest_topology topology) {
    ZEST_ASSERT_HANDLE(pipeline_template);  //invalid pipeline template handle
    pipeline_template->primitive_topology = topology;
}

void zest_SetPipelineCullMode(zest_pipeline_template pipeline_template, zest_cull_mode cull_mode) {
    ZEST_ASSERT_HANDLE(pipeline_template);  //invalid pipeline template handle
    pipeline_template->rasterization.cull_mode = cull_mode;
}

void zest_SetPipelinePolygonFillMode(zest_pipeline_template pipeline_template, zest_polygon_mode polygon_mode) {
    ZEST_ASSERT_HANDLE(pipeline_template);  //invalid pipeline template handle
    pipeline_template->rasterization.polygon_mode = polygon_mode;
}

void zest_SetPipelinePushConstantRange(zest_pipeline_template pipeline_template, zest_uint size, zest_supported_shader_stages stage_flags) {
    ZEST_ASSERT_HANDLE(pipeline_template);  //Not a valid pipeline template handle
    zest_push_constant_range_t range;
    range.size = size;
    range.offset = 0;
    range.stage_flags = stage_flags;
    pipeline_template->push_constant_range = range;
}

void zest_SetPipelinePushConstants(zest_pipeline_template pipeline_template, void *push_constants) {
    ZEST_ASSERT_HANDLE(pipeline_template);  //Not a valid pipeline template handle
    pipeline_template->push_constants = push_constants;
}

void zest_SetPipelineBlend(zest_pipeline_template pipeline_template, zest_color_blend_attachment_t blend_attachment) {
    ZEST_ASSERT_HANDLE(pipeline_template);  //Not a valid pipeline template handle
    pipeline_template->color_blend_attachment = blend_attachment;
}

void zest_SetPipelineDepthTest(zest_pipeline_template pipeline_template, bool enable_test, bool write_enable) {
    ZEST_ASSERT_HANDLE(pipeline_template);  //Not a valid pipeline template handle
	pipeline_template->depth_stencil.depth_test_enable = enable_test;
	pipeline_template->depth_stencil.depth_write_enable = write_enable;
	pipeline_template->depth_stencil.depth_compare_op = zest_compare_op_less;
	pipeline_template->depth_stencil.depth_bounds_test_enable = ZEST_FALSE;
	pipeline_template->depth_stencil.stencil_test_enable = ZEST_FALSE;
}

void zest_SetPipelineEnableVertexInput(zest_pipeline_template pipeline_template) {
	pipeline_template->no_vertex_input = ZEST_FALSE;
}

void zest_SetPipelineDisableVertexInput(zest_pipeline_template pipeline_template) {
	pipeline_template->no_vertex_input = ZEST_TRUE;
}

void zest_AddPipelineDescriptorLayout(zest_pipeline_template pipeline_template, zest_set_layout_handle layout_handle) {
    zest_set_layout layout = (zest_set_layout)zest__get_store_resource_checked(layout_handle.context, layout_handle.value);
    ZEST_ASSERT_HANDLE(pipeline_template);  //Not a valid pipeline template handle
    zest_vec_push(layout_handle.context->device->allocator, pipeline_template->set_layouts, layout);
}

void zest_ClearPipelineDescriptorLayouts(zest_pipeline_template pipeline_template) {
    ZEST_ASSERT_HANDLE(pipeline_template);  //Not a valid pipeline template handle
    zest_vec_clear(pipeline_template->set_layouts);
}

zest_vertex_binding_desc_t zest_AddVertexInputBindingDescription(zest_pipeline_template pipeline_template, zest_uint binding, zest_uint stride, zest_input_rate input_rate) {
    zest_vec_foreach(i, pipeline_template->binding_descriptions) {
        //You already have a binding with that index in the bindindDescriptions array
        //Maybe you copied a template with zest_CopyPipelineTemplate but didn't call zest_ClearVertexInputBindingDescriptions on the copy before
        //adding your own
        ZEST_ASSERT(binding != pipeline_template->binding_descriptions[i].binding);
    }
    zest_vertex_binding_desc_t input_binding_description = ZEST__ZERO_INIT(zest_vertex_binding_desc_t);
    input_binding_description.binding = binding;
    input_binding_description.stride = stride;
    input_binding_description.input_rate = input_rate;
    zest_vec_push(pipeline_template->context->device->allocator, pipeline_template->binding_descriptions, input_binding_description);
    zest_size size = zest_vec_size(pipeline_template->binding_descriptions);
    return input_binding_description;
}

void zest_ClearVertexInputBindingDescriptions(zest_pipeline_template pipeline_template) {
    zest_vec_clear(pipeline_template->binding_descriptions);
}

void zest_ClearVertexAttributeDescriptions(zest_pipeline_template pipeline_template) {
    zest_vec_clear(pipeline_template->attribute_descriptions);
}

void zest_ClearPipelinePushConstantRanges(zest_pipeline_template pipeline_template) {
    pipeline_template->push_constant_range.offset = 0;
    pipeline_template->push_constant_range.size = 0;
    pipeline_template->push_constant_range.stage_flags = 0;
}

zest_color_blend_attachment_t zest_AdditiveBlendState() {
    zest_color_blend_attachment_t color_blend_attachment;
    color_blend_attachment.blend_enable = ZEST_TRUE;
    color_blend_attachment.color_write_mask = zest_color_component_r_bit | zest_color_component_g_bit | zest_color_component_b_bit | zest_color_component_a_bit;
    color_blend_attachment.src_color_blend_factor = zest_blend_factor_one;
    color_blend_attachment.dst_color_blend_factor = zest_blend_factor_one;
    color_blend_attachment.color_blend_op = zest_blend_op_add;
    color_blend_attachment.src_alpha_blend_factor = zest_blend_factor_one;
    color_blend_attachment.dst_alpha_blend_factor = zest_blend_factor_one;
    color_blend_attachment.alpha_blend_op = zest_blend_op_add;
    return color_blend_attachment;
}

zest_color_blend_attachment_t zest_AdditiveBlendState2() {
    zest_color_blend_attachment_t color_blend_attachment;
    color_blend_attachment.blend_enable = ZEST_TRUE;
    color_blend_attachment.color_write_mask = zest_color_component_r_bit | zest_color_component_g_bit | zest_color_component_b_bit | zest_color_component_a_bit;
    color_blend_attachment.src_color_blend_factor = zest_blend_factor_one;
    color_blend_attachment.dst_color_blend_factor = zest_blend_factor_one;
    color_blend_attachment.color_blend_op = zest_blend_op_add;
    color_blend_attachment.src_alpha_blend_factor = zest_blend_factor_src_alpha;
    color_blend_attachment.dst_alpha_blend_factor = zest_blend_factor_dst_alpha;
    color_blend_attachment.alpha_blend_op = zest_blend_op_add;
    return color_blend_attachment;
}

zest_color_blend_attachment_t zest_AlphaOnlyBlendState() {
    zest_color_blend_attachment_t color_blend_attachment;
    color_blend_attachment.blend_enable = ZEST_TRUE;
    color_blend_attachment.color_write_mask = zest_color_component_a_bit;
    color_blend_attachment.src_color_blend_factor = zest_blend_factor_one;
    color_blend_attachment.dst_color_blend_factor = zest_blend_factor_one;
    color_blend_attachment.color_blend_op = zest_blend_op_add;
    color_blend_attachment.src_alpha_blend_factor = zest_blend_factor_one;
    color_blend_attachment.dst_alpha_blend_factor = zest_blend_factor_one_minus_src_alpha;
    color_blend_attachment.alpha_blend_op = zest_blend_op_add;
    return color_blend_attachment;
}

zest_color_blend_attachment_t zest_BlendStateNone() {
    zest_color_blend_attachment_t color_blend_attachment = ZEST__ZERO_INIT(zest_color_blend_attachment_t);
    color_blend_attachment.color_write_mask = 0;
    color_blend_attachment.blend_enable = ZEST_FALSE;
    return color_blend_attachment;
}

zest_color_blend_attachment_t zest_AlphaBlendState() {
    zest_color_blend_attachment_t color_blend_attachment;
    color_blend_attachment.color_write_mask = zest_color_component_r_bit | zest_color_component_g_bit | zest_color_component_b_bit | zest_color_component_a_bit;
    color_blend_attachment.blend_enable = ZEST_TRUE;
    color_blend_attachment.src_color_blend_factor = zest_blend_factor_one;
    color_blend_attachment.dst_color_blend_factor = zest_blend_factor_one_minus_src_alpha;
    color_blend_attachment.color_blend_op = zest_blend_op_add;
    color_blend_attachment.src_alpha_blend_factor = zest_blend_factor_one;
    color_blend_attachment.dst_alpha_blend_factor = zest_blend_factor_one_minus_src_alpha;
    color_blend_attachment.alpha_blend_op = zest_blend_op_add;
    return color_blend_attachment;
}

zest_color_blend_attachment_t zest_PreMultiplyBlendState() {
    zest_color_blend_attachment_t color_blend_attachment;
    color_blend_attachment.color_write_mask = zest_color_component_r_bit | zest_color_component_g_bit | zest_color_component_b_bit | zest_color_component_a_bit;
    color_blend_attachment.blend_enable = ZEST_TRUE;
    color_blend_attachment.src_color_blend_factor = zest_blend_factor_one;
    color_blend_attachment.dst_color_blend_factor = zest_blend_factor_one_minus_src_alpha;
    color_blend_attachment.color_blend_op = zest_blend_op_add;
    color_blend_attachment.src_alpha_blend_factor = zest_blend_factor_one;
    color_blend_attachment.dst_alpha_blend_factor = zest_blend_factor_one_minus_src_alpha;
    color_blend_attachment.alpha_blend_op = zest_blend_op_add;
    return color_blend_attachment;
}

zest_color_blend_attachment_t zest_PreMultiplyBlendStateForSwap() {
    zest_color_blend_attachment_t color_blend_attachment;
    color_blend_attachment.color_write_mask = zest_color_component_r_bit | zest_color_component_g_bit | zest_color_component_b_bit | zest_color_component_a_bit;
    color_blend_attachment.blend_enable = ZEST_TRUE;
    color_blend_attachment.src_color_blend_factor = zest_blend_factor_one;
    color_blend_attachment.dst_color_blend_factor = zest_blend_factor_one_minus_src_alpha;
    color_blend_attachment.color_blend_op = zest_blend_op_add;
    color_blend_attachment.src_alpha_blend_factor = zest_blend_factor_one;
    color_blend_attachment.dst_alpha_blend_factor = zest_blend_factor_zero;
    color_blend_attachment.alpha_blend_op = zest_blend_op_add;
    return color_blend_attachment;
}

zest_color_blend_attachment_t zest_MaxAlphaBlendState() {
    zest_color_blend_attachment_t color_blend_attachment;
    color_blend_attachment.color_write_mask = zest_color_component_r_bit | zest_color_component_g_bit | zest_color_component_b_bit | zest_color_component_a_bit;
    color_blend_attachment.blend_enable = ZEST_TRUE;
    color_blend_attachment.src_color_blend_factor = zest_blend_factor_src_alpha;
    color_blend_attachment.dst_color_blend_factor = zest_blend_factor_one_minus_dst_alpha;
    color_blend_attachment.color_blend_op = zest_blend_op_add;
    color_blend_attachment.src_alpha_blend_factor = zest_blend_factor_one;
    color_blend_attachment.dst_alpha_blend_factor = zest_blend_factor_one_minus_src_alpha;
    color_blend_attachment.alpha_blend_op = zest_blend_op_add;
    return color_blend_attachment;
}

zest_color_blend_attachment_t zest_ImGuiBlendState() {
    zest_color_blend_attachment_t color_blend_attachment;
    color_blend_attachment.color_write_mask = zest_color_component_r_bit | zest_color_component_g_bit | zest_color_component_b_bit | zest_color_component_a_bit;
    color_blend_attachment.blend_enable = ZEST_TRUE;
    color_blend_attachment.src_color_blend_factor = zest_blend_factor_src_alpha;
    color_blend_attachment.dst_color_blend_factor = zest_blend_factor_one_minus_src_alpha;
    color_blend_attachment.color_blend_op = zest_blend_op_add;
    color_blend_attachment.src_alpha_blend_factor = zest_blend_factor_one_minus_src_alpha;
    color_blend_attachment.dst_alpha_blend_factor = zest_blend_factor_zero;
    color_blend_attachment.alpha_blend_op = zest_blend_op_add;
    return color_blend_attachment;
}

zest_bool zest__cache_pipeline(zest_pipeline_template pipeline_template, zest_command_list command_list, zest_key pipeline_key, zest_pipeline *out_pipeline) {
	zest_context context = command_list->context;
	zest_pipeline pipeline = zest__create_pipeline(context);
    pipeline->pipeline_template = pipeline_template;
    zest_bool result = context->device->platform->build_pipeline(pipeline, command_list);
	if (result == ZEST_TRUE) {
		zest_map_insert_key(pipeline_template->context->device->allocator, context->device->cached_pipelines, pipeline_key, pipeline);
		zest_vec_push(pipeline_template->context->device->allocator, pipeline_template->cached_pipeline_keys, pipeline_key);
		*out_pipeline = pipeline;
	} else {
		ZEST__FLAG(pipeline_template->flags, zest_pipeline_invalid);
		zest__cleanup_pipeline(pipeline);
	}
    return result;
}

zest_pipeline_template zest_CopyPipelineTemplate(zest_context context, const char *name, zest_pipeline_template pipeline_to_copy) {
    zest_pipeline_template copy = zest_BeginPipelineTemplate(context, name);
    copy->no_vertex_input = pipeline_to_copy->no_vertex_input;
    copy->primitive_topology = pipeline_to_copy->primitive_topology;
    copy->rasterization = pipeline_to_copy->rasterization;
	copy->color_blend_attachment = pipeline_to_copy->color_blend_attachment;
    copy->push_constant_range = pipeline_to_copy->push_constant_range;
    zest_vec_clear(copy->set_layouts);
    zest_vec_foreach(i, pipeline_to_copy->set_layouts) {
        zest_vec_push(context->device->allocator, copy->set_layouts, pipeline_to_copy->set_layouts[i]);
    }
    if (pipeline_to_copy->binding_descriptions) {
        zest_vec_resize(context->device->allocator, copy->binding_descriptions, zest_vec_size(pipeline_to_copy->binding_descriptions));
        memcpy(copy->binding_descriptions, pipeline_to_copy->binding_descriptions, zest_vec_size_in_bytes(pipeline_to_copy->binding_descriptions));
    }
    if (pipeline_to_copy->attribute_descriptions) {
        zest_vec_resize(context->device->allocator, copy->attribute_descriptions, zest_vec_size(pipeline_to_copy->attribute_descriptions));
        memcpy(copy->attribute_descriptions, pipeline_to_copy->attribute_descriptions, zest_vec_size_in_bytes(pipeline_to_copy->attribute_descriptions));
    }
    copy->vertex_shader = pipeline_to_copy->vertex_shader;
    copy->fragment_shader = pipeline_to_copy->fragment_shader;
    return copy;
}

void zest_FreePipelineTemplate(zest_pipeline_template pipeline_template) {
    ZEST_ASSERT_HANDLE(pipeline_template);   //Not a valid pipeline template handle
	zest_context context = pipeline_template->context;
	zest_vec_foreach(i, pipeline_template->cached_pipeline_keys) {
		zest_key key = pipeline_template->cached_pipeline_keys[i];
		if (zest_map_valid_key(context->device->cached_pipelines, key)) {
			zest_pipeline pipeline = *zest_map_at_key(context->device->cached_pipelines, key);
            zest_vec_push(context->device->allocator, context->device->deferred_resource_freeing_list.resources[context->current_fif], pipeline);
		}
	}
    zest__cleanup_pipeline_template(pipeline_template);
}

zest_bool zest_PipelineIsValid(zest_pipeline_template pipeline) {
	return ZEST__NOT_FLAGGED(pipeline->flags, zest_pipeline_invalid);
}

void zest__cleanup_pipeline_template(zest_pipeline_template pipeline_template) {
    zest_vec_free(pipeline_template->context->device->allocator, pipeline_template->set_layouts);
    zest_vec_free(pipeline_template->context->device->allocator, pipeline_template->attribute_descriptions);
    zest_vec_free(pipeline_template->context->device->allocator, pipeline_template->binding_descriptions);
    zest_vec_free(pipeline_template->context->device->allocator, pipeline_template->cached_pipeline_keys);
    ZEST__FREE(pipeline_template->context->device->allocator, pipeline_template);
}

zest_bool zest_ValidateShader(zest_context context, const char *shader_code, zest_shader_type type, const char *name) {
	if (!context->device->platform->validate_shader(context, shader_code, type, name)) {
		return ZEST_FALSE;
	}
    return ZEST_TRUE;
}

zest_bool zest_CompileShader(zest_shader_handle shader_handle) {
	zest_context context = shader_handle.context;
    zest_shader shader = (zest_shader)zest__get_store_resource_checked(context, shader_handle.value);
	
    if (context->device->platform->compile_shader(shader, shader->shader_code.str, zest_TextLength(&shader->shader_code), shader->type, shader->name.str, "main", NULL)) {
		ZEST_APPEND_LOG(context->device->log_path.str, "Successfully compiled shader: %s.", shader->name.str);
        return ZEST_TRUE;
    }
    return ZEST_FALSE;
}

zest_shader_handle zest_CreateShaderFromFile(zest_context context, const char *file, const char *name, zest_shader_type type, zest_bool disable_caching) {
    char *shader_code = zest_ReadEntireFile(context, file, ZEST_TRUE);
    ZEST_ASSERT(shader_code);   //Unable to load the shader code, check the path is valid
    zest_shader_handle shader_handle = zest_CreateShader(context, shader_code, type, name, disable_caching);
	zest_shader shader = (zest_shader)zest__get_store_resource_checked(context, shader_handle.value);
    zest_SetText(context->device->allocator, &shader->file_path, file);
    zest_vec_free(context->device->allocator, shader_code);
    return shader_handle;
}

zest_bool zest_ReloadShader(zest_shader_handle shader_handle) {
	zest_shader shader = (zest_shader)zest__get_store_resource_checked(shader_handle.context, shader_handle.value);
    ZEST_ASSERT(zest_TextLength(&shader->file_path));    //The shader must have a file path set.
    char *shader_code = zest_ReadEntireFile(shader_handle.context, shader->file_path.str, ZEST_TRUE);
    if (!shader_code) {
        return 0;
    }
    zest_SetText(shader_handle.context->device->allocator, &shader->shader_code, shader_code);
    return 1;
}

zest_shader_handle zest_CreateShader(zest_context context, const char *shader_code, zest_shader_type type, const char *name, zest_bool disable_caching) {
    ZEST_ASSERT(name);     //You must give the shader a name
    zest_text_t shader_name = ZEST__ZERO_INIT(zest_text_t);
    if (zest_TextSize(&context->device->shader_path_prefix)) {
        zest_SetTextf(context->device->allocator, &shader_name, "%s%s", context->device->shader_path_prefix, name);
    }
    else {
        zest_SetTextf(context->device->allocator, &shader_name, "%s", name);
    }
    zest_shader_handle shader_handle = zest__new_shader(context, type);
    zest_shader shader = (zest_shader)zest__get_store_resource_checked(context, shader_handle.value);
    shader->name = shader_name;
    if (!disable_caching && context->create_info.flags & zest_init_flag_cache_shaders) {
        shader->spv = zest_ReadEntireFile(context, shader->name.str, ZEST_FALSE);
        if (shader->spv) {
            shader->spv_size = zest_vec_size(shader->spv);
			zest_SetText(context->device->allocator, &shader->shader_code, shader_code);
			ZEST_APPEND_LOG(context->device->log_path.str, "Loaded shader %s from cache.", name);
            return shader_handle;
        }
    }

	zest_SetText(context->device->allocator, &shader->shader_code, shader_code);
	if (!context->device->platform->compile_shader(shader, shader->shader_code.str, zest_TextLength(&shader->shader_code), type, name, "main", NULL)) {
        zest_FreeShader(shader_handle);
        ZEST_ASSERT(0); //There's a bug in this shader that needs fixing. You can check the log file for the error message
	}

    if (!disable_caching && context->create_info.flags & zest_init_flag_cache_shaders) {
        zest__cache_shader(shader);
    }
    return shader_handle;
}

void zest__cache_shader(zest_shader shader) {
	zest_context context = shader->handle.context;
    zest__create_folder(context, context->device->shader_path_prefix.str);
    FILE *shader_file = zest__open_file(shader->name.str, "wb");
    if (shader_file == NULL) {
        ZEST_APPEND_LOG(context->device->log_path.str, "Failed to open file for writing: %s", shader->name.str);
        return;
    }
    size_t written = fwrite(shader->spv, 1, shader->spv_size, shader_file);
    if (written != shader->spv_size) {
        ZEST_APPEND_LOG(context->device->log_path.str, "Failed to write entire shader to file: %s", shader->name.str);
        fclose(shader_file);
    }
    fclose(shader_file);
}

zest_shader_handle zest_CreateShaderSPVMemory(zest_context context, const unsigned char *shader_code, zest_uint spv_length, const char *name, zest_shader_type type) {
    ZEST_ASSERT(shader_code);   //No shader code!
    ZEST_ASSERT(name);     //You must give the shader a name
    zest_text_t shader_name = ZEST__ZERO_INIT(zest_text_t);
    if (zest_TextSize(&context->device->shader_path_prefix)) {
        zest_SetTextf(context->device->allocator, &shader_name, "%s%s", context->device->shader_path_prefix, name);
    } else {
        zest_SetTextf(context->device->allocator, &shader_name, "%s", name);
    }
    zest_shader_handle shader_handle = zest__new_shader(context, type);
    zest_shader shader = (zest_shader)zest__get_store_resource_checked(context, shader_handle.value);
    zest_vec_resize(context->device->allocator, shader->spv, spv_length);
    memcpy(shader->spv, shader_code, spv_length);
    shader->spv_size = spv_length;
    zest_FreeText(context->device->allocator, &shader_name);
    return shader_handle;
}

void zest__compile_builtin_shaders(zest_context context) {
    context->device->builtin_shaders.swap_vert          = zest_CreateShader(context, zest_shader_swap_vert, zest_vertex_shader, "swap_vert.spv", 1);
    context->device->builtin_shaders.swap_frag          = zest_CreateShader(context, zest_shader_swap_frag, zest_fragment_shader, "swap_frag.spv", 1);
}

zest_shader_handle zest__new_shader(zest_context context, zest_shader_type type) {
    zest_shader_handle handle = ZEST_STRUCT_LITERAL(zest_shader_handle, zest__add_store_resource(zest_handle_type_shaders, context) );
    zest_shader shader = (zest_shader)zest__get_store_resource_checked(context, handle.value);
    *shader = ZEST__ZERO_INIT(zest_shader_t);
    shader->magic = zest_INIT_MAGIC(zest_struct_type_shader);
    shader->type = type;
	handle.context = context;
    shader->handle = handle;
    return handle;
}

zest_shader_handle zest_AddShaderFromSPVFile(zest_context context, const char *filename, zest_shader_type type) {
    ZEST_ASSERT(filename);     //You must give the shader a name
    zest_shader_handle shader_handle = zest__new_shader(context, type);
    zest_shader shader = (zest_shader)zest__get_store_resource_checked(context, shader_handle.value);
    shader->spv = zest_ReadEntireFile(context, filename, ZEST_FALSE);
    ZEST_ASSERT(shader->spv);   //File not found, could not load this shader!
    shader->spv_size = zest_vec_size(shader->spv);
	zest_SetText(context->device->allocator, &shader->name, filename);
	ZEST_APPEND_LOG(context->device->log_path.str, "Loaded shader %s and added to renderer shaders.", filename);
	return shader_handle;
}

zest_shader_handle zest_AddShaderFromSPVMemory(zest_context context, const char *name, const void *buffer, zest_uint size, zest_shader_type type) {
    ZEST_ASSERT(name);     //You must give the shader a name
    ZEST_ASSERT(!strstr(name, "/"));    //name must not contain /, the shader will be prefixed with the cache folder automatically
    if (buffer && size) {
		zest_shader_handle shader_handle = zest__new_shader(context, type);
		zest_shader shader = (zest_shader)zest__get_store_resource_checked(context, shader_handle.value);
		if (zest_TextSize(&context->device->shader_path_prefix)) {
			zest_SetTextf(context->device->allocator, &shader->name, "%s%s", context->device->shader_path_prefix, name);
		}
		else {
			zest_SetTextf(context->device->allocator, &shader->name, "%s", name);
		}
		zest_vec_resize(context->device->allocator, shader->spv, size);
		memcpy(shader->spv, buffer, size);
        ZEST_APPEND_LOG(context->device->log_path.str, "Read shader %s from memory and added to renderer shaders.", name);
        shader->spv_size = size;
        return shader_handle;
    }
    return ZEST__ZERO_INIT(zest_shader_handle);
}

void zest_AddShader(zest_shader_handle shader_handle, const char *name) {
	zest_shader shader = (zest_shader)zest__get_store_resource_checked(shader_handle.context, shader_handle.value);
	zest_context context = shader_handle.context;
    if (name) {
		ZEST_ASSERT(!strstr(name, "/"));    //name must not contain /, the shader will be prefixed with the cache folder automatically
        if (zest_TextSize(&context->device->shader_path_prefix)) {
            zest_SetTextf(context->device->allocator, &shader->name, "%s%s", context->device->shader_path_prefix, name);
        }
        else {
            zest_SetTextf(context->device->allocator, &shader->name, "%s", name);
        }
    }
    else {
        ZEST_ASSERT(shader->name.str);
    }
}

void zest_FreeShader(zest_shader_handle shader_handle) {
	zest_shader shader = (zest_shader)zest__get_store_resource_checked(shader_handle.context, shader_handle.value);
    zest_FreeText(shader_handle.context->device->allocator, &shader->name);
    zest_FreeText(shader_handle.context->device->allocator, &shader->shader_code);
    zest_FreeText(shader_handle.context->device->allocator, &shader->file_path);
    if (shader->spv) {
        zest_vec_free(shader_handle.context->device->allocator, shader->spv);
    }
    zest__remove_store_resource(shader_handle.context, shader_handle.value);
}

zest_uint zest_GetMaxImageSize(zest_context context) {
	ZEST_ASSERT_HANDLE(context);
	return context->device->max_image_size;
}

zest_uint zest_GetLayerVertexDescriptorIndex(zest_layer_handle layer_handle, bool last_frame) {
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(layer_handle.context, layer_handle.value);
    return layer->memory_refs[last_frame ? layer->prev_fif : layer->fif].device_vertex_data->array_index;
}

zest_pipeline_template zest_BeginPipelineTemplate(zest_context context, const char* name) {
    zest_pipeline_template pipeline_template = (zest_pipeline_template)ZEST__NEW(context->device->allocator, zest_pipeline_template);
    *pipeline_template = ZEST__ZERO_INIT(zest_pipeline_template_t);
    pipeline_template->magic = zest_INIT_MAGIC(zest_struct_type_pipeline_template);
    pipeline_template->name = name;
    pipeline_template->no_vertex_input = ZEST_FALSE;
    pipeline_template->fragShaderFunctionName = "main";
    pipeline_template->vertShaderFunctionName = "main";
    pipeline_template->color_blend_attachment = zest_AlphaBlendState();

    pipeline_template->rasterization.depth_clamp_enable = ZEST_FALSE;
    pipeline_template->rasterization.rasterizer_discard_enable = ZEST_FALSE;
    pipeline_template->rasterization.polygon_mode = zest_polygon_mode_fill;
	pipeline_template->primitive_topology = zest_topology_triangle_list;
    pipeline_template->rasterization.line_width = 1.0f;
    pipeline_template->rasterization.cull_mode = zest_cull_mode_none;
    pipeline_template->rasterization.front_face = zest_front_face_clockwise;
    pipeline_template->rasterization.depth_bias_enable = ZEST_FALSE;

	pipeline_template->context = context;

    return pipeline_template;
}

void zest_AddVertexAttribute(zest_pipeline_template pipeline_template, zest_uint binding, zest_uint location, zest_format format, zest_uint offset) {
    zest_vertex_attribute_desc_t input_attribute_description = ZEST__ZERO_INIT(zest_vertex_attribute_desc_t);
    input_attribute_description.location = location;
    input_attribute_description.binding = binding;
    input_attribute_description.format = format;
    input_attribute_description.offset = offset;
    zest_vec_push(pipeline_template->context->device->allocator, pipeline_template->attribute_descriptions, input_attribute_description);
}

zest_key zest_Hash(const void* input, zest_ull length, zest_ull seed) { 
    zest_hasher_t hasher;
    zest__hash_initialise(&hasher, seed); 
    zest__hasher_add(&hasher, input, length); 
    return (zest_key)zest__get_hash(&hasher); 
}

zest_pipeline zest_PipelineWithTemplate(zest_pipeline_template pipeline_template, const zest_command_list command_list) {
	zest_context context = command_list->context;
    if (zest_vec_size(pipeline_template->set_layouts) == 0) {
        ZEST_PRINT("ERROR: You're trying to build a pipeline (%s) that has no descriptor set layouts configured. You can add descriptor layouts when building the pipeline with zest_AddPipelineTemplateDescriptorLayout.", pipeline_template->name);
        return NULL;
    }
	if (!zest_PipelineIsValid(pipeline_template)) {
		ZEST__REPORT(context, zest_report_unused_pass, "You're trying to build a pipeline (%s) that has been marked as invalid. This means that the last time this pipeline was created it failed with errors. You can check for validation errors to see what they were.", pipeline_template->name);
		return NULL;
	}
    zest_key pipeline_key = (zest_key)pipeline_template;
	zest_key cached_pipeline_key = zest_Hash(&command_list->rendering_info, sizeof(zest_rendering_info_t), pipeline_key);
    if (zest_map_valid_key(context->device->cached_pipelines, pipeline_key)) {
		return *zest_map_at_key(context->device->cached_pipelines, pipeline_key); 
    }
    zest_pipeline pipeline = 0;
	if (!zest__cache_pipeline(pipeline_template, command_list, pipeline_key, &pipeline)) {
		ZEST_PRINT("ERROR: Unable to build and cache pipeline [%s]. Check the log for the most recent errors.", pipeline_template->name);
	}
	return pipeline;
}

zest_extent2d_t zest_GetSwapChainExtent(zest_context context) { return context->swapchain->size; }
zest_uint zest_ScreenWidth(zest_context context) { return (zest_uint)context->swapchain->size.width; }
zest_uint zest_ScreenHeight(zest_context context) { return (zest_uint)context->swapchain->size.height; }
float zest_ScreenWidthf(zest_context context) { return (float)context->swapchain->size.width; }
float zest_ScreenHeightf(zest_context context) { return (float)context->swapchain->size.height; }
void *zest_NativeWindow(zest_context context) { return context->window_data.native_handle; }
void *zest_Window(zest_context context) { return context->window_data.window_handle; }
float zest_DPIScale(zest_context context) { return context->dpi_scale; }
void zest_SetDPIScale(zest_context context, float scale) { context->dpi_scale = scale; }
void zest_WaitForIdleDevice(zest_context context) { context->device->platform->wait_for_idle_device(context); }
void zest__hash_initialise(zest_hasher_t* hasher, zest_ull seed) { hasher->state[0] = seed + zest__PRIME1 + zest__PRIME2; hasher->state[1] = seed + zest__PRIME2; hasher->state[2] = seed; hasher->state[3] = seed - zest__PRIME1; hasher->buffer_size = 0; hasher->total_length = 0; }

zest_set_layout_handle zest_GetUniformBufferLayout(zest_uniform_buffer_handle handle) {
    zest_uniform_buffer uniform_buffer = (zest_uniform_buffer)zest__get_store_resource_checked(handle.context, handle.value);
    return uniform_buffer->set_layout;
}

zest_descriptor_set zest_GetUniformBufferSet(zest_uniform_buffer_handle handle) {
	zest_context context = handle.context;
    zest_uniform_buffer uniform_buffer = (zest_uniform_buffer)zest__get_store_resource_checked(context, handle.value);
    return uniform_buffer->descriptor_set[context->current_fif];
}

zest_descriptor_set zest_GetFIFUniformBufferSet(zest_uniform_buffer_handle handle, zest_uint fif) {
    ZEST_ASSERT(fif < ZEST_MAX_FIF);
    zest_uniform_buffer uniform_buffer = (zest_uniform_buffer)zest__get_store_resource_checked(handle.context, handle.value);
    return uniform_buffer->descriptor_set[fif];
}

void* zest_GetUniformBufferData(zest_uniform_buffer_handle handle) {
	zest_context context = handle.context;
    zest_uniform_buffer uniform_buffer = (zest_uniform_buffer)zest__get_store_resource_checked(handle.context, handle.value);
    return uniform_buffer->buffer[context->current_fif]->data;
}

zest_uint zest_GetBufferDescriptorIndex(zest_buffer buffer) {
    ZEST_ASSERT(buffer);  //Not a valid buffer handle
    return buffer->array_index;
}

void *zest_BufferData(zest_buffer buffer) {
    ZEST_ASSERT(buffer);  //Not a valid buffer handle
    return buffer->data;
}

zest_size zest_BufferSize(zest_buffer buffer) {
    ZEST_ASSERT(buffer);  //Not a valid buffer handle
    return buffer->size;
}

zest_size zest_BufferMemoryInUse(zest_buffer buffer) {
    ZEST_ASSERT(buffer);  //Not a valid buffer handle
    return buffer->memory_in_use;
}

void zest_SetBufferMemoryInUse(zest_buffer buffer, zest_size size) {
    ZEST_ASSERT(buffer);  //Not a valid buffer handle
    ZEST_ASSERT(size <= buffer->size);  //You're trying to set to the memory in use to a size larger than the buffer size
    buffer->memory_in_use = size;
}


void zest_ResetCompute(zest_compute_handle compute_handle) {
    zest_compute compute = (zest_compute)zest__get_store_resource_checked(compute_handle.context, compute_handle.value);
    compute->fif = (compute->fif + 1) % ZEST_MAX_FIF;
}

void zest_FreeCompute(zest_compute_handle compute_handle) {
	zest_context context = compute_handle.context;
    zest_compute compute = (zest_compute)zest__get_store_resource_checked(context, compute_handle.value);
    zest_vec_push(context->device->allocator, context->device->deferred_resource_freeing_list.resources[context->current_fif], compute);
}

int zest_ComputeConditionAlwaysTrue(zest_compute_handle layer) {
    return 1;
}

void zest_EnableVSync(zest_context context) {
    ZEST__FLAG(context->flags, zest_context_flag_vsync_enabled);
    ZEST__FLAG(context->flags, zest_context_flag_schedule_change_vsync);
}

void zest_DisableVSync(zest_context context) {
    ZEST__UNFLAG(context->flags, zest_context_flag_vsync_enabled);
    ZEST__FLAG(context->flags, zest_context_flag_schedule_change_vsync);
}

void zest_SetFrameInFlight(zest_context context, zest_uint fif) {
    ZEST_ASSERT(fif < ZEST_MAX_FIF);        //Frame in flight must be less than the maximum amount of frames in flight
    context->saved_fif = context->current_fif;
    context->current_fif = fif;
}

void zest_RestoreFrameInFlight(zest_context context) {
    context->current_fif = context->saved_fif;
}

float zest_LinearToSRGB(float value) {
    return powf(value, 2.2f);
}

zest_uint zest__grow_capacity(void* T, zest_uint size) {
    zest_uint new_capacity = T ? (size + size / 2) : 8;
    return new_capacity > size ? new_capacity : size;
}

void* zest__vec_reserve(zloc_allocator *allocator, void* T, zest_uint unit_size, zest_uint new_capacity) {
    if (T && new_capacity <= zest__vec_header(T)->capacity) {
        return T;
    }
    void* new_data = ZEST__REALLOCATE(allocator, (T ? zest__vec_header(T) : T), new_capacity * unit_size + zest__VEC_HEADER_OVERHEAD);
    if (!T) memset(new_data, 0, zest__VEC_HEADER_OVERHEAD);
    T = ((char*)new_data + zest__VEC_HEADER_OVERHEAD);
    zest_vec *header = (zest_vec *)new_data;
    if (allocator->user_data) {
        zest_device device = (zest_device)allocator->user_data;
        header->id = device->vector_id++;
        if (header->id == 166) {
            int d = 0;
        }
    }
    header->magic = zest_INIT_MAGIC(zest_struct_type_vector);
    header->capacity = new_capacity;
    return T;
}

void *zest__vec_linear_reserve(zloc_linear_allocator_t *allocator, void *T, zest_uint unit_size, zest_uint new_capacity) {
	if (T && new_capacity <= zest__vec_header(T)->capacity) {
		return T;
	}
    void* new_data = zloc_LinearAllocation(allocator, new_capacity * unit_size + zest__VEC_HEADER_OVERHEAD);
    zest_vec *header = (zest_vec *)new_data;
	if (!T) {
		memset(new_data, 0, zest__VEC_HEADER_OVERHEAD);
	} else {
		zest_vec *current_header = zest__vec_header(T);
		memcpy(((char*)new_data + zest__VEC_HEADER_OVERHEAD), T, current_header->capacity * unit_size);
		header->current_size = current_header->current_size;
	}
    header->magic = zest_INIT_MAGIC(zest_struct_type_vector);
    header->capacity = new_capacity;
    return ((char*)new_data + zest__VEC_HEADER_OVERHEAD);
}

void zest__initialise_bucket_array(zest_context context, zest_bucket_array_t *array, zest_uint element_size, zest_uint bucket_capacity) {
    array->buckets = NULL; // zest_vec will handle initialization on first push
    array->bucket_capacity = bucket_capacity;
    array->current_size = 0;
    array->element_size = element_size;
	array->context = context;
}

void zest__free_bucket_array(zest_bucket_array_t *array) {
    if (!array || !array->buckets) return;
    // Free each individual bucket
    zest_vec_foreach(i, array->buckets) {
        ZEST__FREE(array->context->device->allocator, array->buckets[i]);
    }
    // Free the zest_vec that holds the bucket pointers
    zest_vec_free(array->context->device->allocator, array->buckets);
    array->buckets = NULL;
    array->current_size = 0;
}

void *zest__bucket_array_get(zest_bucket_array_t *array, zest_uint index) {
    if (!array || index >= array->current_size) {
        return NULL;
    }
    zest_uint bucket_index = index / array->bucket_capacity;
    zest_uint index_in_bucket = index % array->bucket_capacity;
    return (void *)((char *)array->buckets[bucket_index] + index_in_bucket * array->element_size);
}

void *zest__bucket_array_add(zest_bucket_array_t *array) {
    ZEST_ASSERT(array);
    // If the array is empty or the last bucket is full, allocate a new one.
    if (zest_vec_empty(array->buckets) || (array->current_size % array->bucket_capacity == 0)) {
        void *new_bucket = ZEST__ALLOCATE(array->context->device->allocator, array->element_size * array->bucket_capacity);
        zest_vec_push(array->context->device->allocator, array->buckets, new_bucket);
    }

    // Get the pointer to the new element's location
    zest_uint bucket_index = (array->current_size) / array->bucket_capacity;
    zest_uint index_in_bucket = (array->current_size) % array->bucket_capacity;
    void *new_element = (void *)((char *)array->buckets[bucket_index] + index_in_bucket * array->element_size);

    array->current_size++;
    return new_element;
}

inline void *zest__bucket_array_linear_add(zloc_linear_allocator_t *allocator, zest_bucket_array_t *array) {
    ZEST_ASSERT(array);
    // If the array is empty or the last bucket is full, allocate a new one.
    if (zest_vec_empty(array->buckets) || (array->current_size % array->bucket_capacity == 0)) {
		void *new_bucket = zloc_LinearAllocation(allocator, array->element_size * array->bucket_capacity);
        zest_vec_linear_push(allocator, array->buckets, new_bucket);
    }

    // Get the pointer to the new element's location
    zest_uint bucket_index = (array->current_size) / array->bucket_capacity;
    zest_uint index_in_bucket = (array->current_size) % array->bucket_capacity;
    void *new_element = (void *)((char *)array->buckets[bucket_index] + index_in_bucket * array->element_size);

    array->current_size++;
    return new_element;
}

void zest__free_store(zest_resource_store_t *store) { 
    if (store->data) { 
		ZEST__FREE(store->context->store_allocator, store->data); 
        zest_vec_free(store->context->store_allocator, store->generations);
        zest_vec_free(store->context->store_allocator, store->free_slots);
        *store = ZEST__ZERO_INIT(zest_resource_store_t);
    } 
}

void zest__reserve_store(zest_resource_store_t *store, zest_uint new_capacity) {
    ZEST_ASSERT(store->struct_size);	//Must assign a value to struct size
    if (new_capacity <= store->capacity) return;
    void *new_data;
    if (store->alignment != 0) {
        new_data = ZEST__ALLOCATE_ALIGNED(store->context->store_allocator, (size_t)new_capacity * store->struct_size, store->alignment);
    } else {
        new_data = ZEST__REALLOCATE(store->context->store_allocator, store->data, (size_t)new_capacity * store->struct_size);
    }
    ZEST_ASSERT(new_data);    //Unable to allocate memory. todo: better handling
    if (store->data) {
        memcpy(new_data, store->data, (size_t)store->current_size * store->struct_size);
        ZEST__FREE(store->context->store_allocator, store->data);
    }
    store->data = new_data;
    store->capacity = new_capacity;
}

void zest__clear_store(zest_resource_store_t *store) {
    if (store->data) {
        store->current_size = 0;
    }
}

zest_uint zest__grow_store_capacity(zest_resource_store_t *store, zest_uint size) {
    zest_uint new_capacity = store->capacity ? (store->capacity + store->capacity / 2) : 8;
    return new_capacity > size ? new_capacity : size;
}

void zest__resize_store(zest_resource_store_t *store, zest_uint new_size) {
    if (new_size > store->capacity) zest__reserve_store(store, zest__grow_store_capacity(store, new_size));
    store->current_size = new_size;
}

void zest__resize_bytes_store(zest_resource_store_t *store, zest_uint new_size) {
    if (new_size > store->capacity) zest__reserve_store(store, zest__grow_store_capacity(store, new_size));
    store->current_size = new_size;
}

zest_uint zest__size_in_bytes_store(zest_resource_store_t *store) {
    return store->current_size * store->struct_size;
}

zest_handle zest__add_store_resource(zest_handle_type type, zest_context context) {
	zest_uint index;                                                                                                           
	zest_uint generation;                                                                                                      
	ZEST_ASSERT(type < zest_max_handle_type);	//Invalid handle type, is the handle valid?
	zest_resource_store_t *store = &context->resource_stores[type];
	if (zest_vec_size(store->free_slots) > 0) {
		index = zest_vec_back(store->free_slots);                                                                          
		zest_vec_pop(store->free_slots);                                                                                  
		generation = ++store->generations[index]; 
	} else {
		index = store->current_size;                                                                                             
		zest_vec_push(context->store_allocator, store->generations, 1);                                                                             
		generation = 1;                                                                       
		zest__resize_store(store, zest_vec_size(store->generations));
		char *position = (char *)store->data + index * store->struct_size;
	}                                                                                                                         
	return ZEST_STRUCT_LITERAL(zest_handle, ZEST_CREATE_HANDLE(type, generation, index));
}

void zest__remove_store_resource(zest_context context, zest_handle handle) {
	zest_handle_type type = ZEST_HANDLE_TYPE(handle);
	ZEST_ASSERT(type < zest_max_handle_type);	//Invalid handle type, is the handle valid?
	zest_resource_store_t *store = &context->resource_stores[type];
	zest_uint index = ZEST_HANDLE_INDEX(handle);                                                                     
	zest_vec_push(context->store_allocator, store->free_slots, index);                                                                               
}                                                                                                                             


void zest__initialise_store(zest_context context, zest_resource_store_t *store, zest_uint struct_size) {
    ZEST_ASSERT(!store->data);   //Must be an empty store
    store->struct_size = struct_size;
    store->alignment = 16;
	store->context = context;
}

void zest_SetText(zloc_allocator *allocator, zest_text_t* buffer, const char* text) {
    if (!text) {
        zest_FreeText(allocator, buffer);
        return;
    }
    zest_uint length = (zest_uint)strlen(text) + 1;
    zest_vec_resize(allocator, buffer->str, length);
    zest_strcpy(buffer->str, length, text);
}

void zest_ResizeText(zloc_allocator *allocator, zest_text_t *buffer, zest_uint size) {
    zest_vec_resize(allocator, buffer->str, size);
}

void zest_SetTextfv(zloc_allocator *allocator, zest_text_t* buffer, const char* text, va_list args)
{
    va_list args2;
    va_copy(args2, args);

#ifdef _MSC_VER
    int len = vsnprintf(NULL, 0, text, args);
    ZEST_ASSERT(len >= 0);

    if ((int)zest_TextSize(buffer) < len + 1)
    {
        zest_vec_resize(allocator, buffer->str, (zest_uint)(len + 1));
    }
    vsnprintf(buffer->str, len + 1, text, args2);
#else
    int len = vsnprintf(buffer->str ? buffer->str : NULL, (size_t)zest_TextSize(buffer), text, args);
    ZEST_ASSERT(len >= 0);

    if (zest_TextSize(buffer) < len + 1)
    {
        zest_vec_resize(allocator, buffer->str, len + 1);
        vsnprintf(buffer->str, (size_t)len + 1, text, args2);
    }
#endif

    ZEST_ASSERT(buffer->str);
}

void zest_AppendTextf(zloc_allocator *allocator, zest_text_t *buffer, const char *format, ...) {
    va_list args;
    va_start(args, format);

    va_list args_copy;
    va_copy(args_copy, args);

    int len = vsnprintf(NULL, 0, format, args);
    ZEST_ASSERT(len >= 0);
    if (len <= 0)
    {
        va_end(args_copy);
        return;
    }

    if (zest_TextSize(buffer)) {
        if (zest_vec_back(buffer->str) == '\0') {
            zest_vec_pop(buffer->str);
        }
    }

    const zest_uint write_off = zest_TextSize(buffer);
    const zest_uint needed_sz = write_off + len + 1;
	zest_vec_resize(allocator, buffer->str, needed_sz);
    vsnprintf(buffer->str + write_off, len + 1, format, args_copy);
    va_end(args_copy);

    va_end(args);
}

void zest_SetTextf(zloc_allocator *allocator, zest_text_t* buffer, const char* text, ...) {
    va_list args;
    va_start(args, text);
    zest_SetTextfv(allocator, buffer, text, args);
    va_end(args);
}

void zest_FreeText(zloc_allocator *allocator, zest_text_t* buffer) {
    zest_vec_free(allocator, buffer->str);
    buffer->str = ZEST_NULL;
}

zest_uint zest_TextLength(zest_text_t* buffer) {
    return (zest_uint)strlen(buffer->str);
}

zest_uint zest_TextSize(zest_text_t* buffer) {
    return zest_vec_size(buffer->str);
}
//End api functions

void zest__log_entry_v(char *str, const char *text, va_list args)
{
    va_list args2;
    va_copy(args2, args);

#ifdef _MSC_VER
    int len = vsnprintf(NULL, 0, text, args);
    ZEST_ASSERT(len >= 0 && len + 1 < ZEST_LOG_ENTRY_SIZE); //log entry must be within buffer limit

    vsnprintf(str, len + 1, text, args2);
#else
    int len = vsnprintf(str ? str : NULL, (size_t)ZEST_LOG_ENTRY_SIZE, text, args);
    ZEST_ASSERT(len >= 0 && len + 1 < ZEST_LOG_ENTRY_SIZE); //log entry must be within buffer limit

	vsnprintf(buffer->str, (size_t)len + 1, text, args2);
#endif

    ZEST_ASSERT(str);
}

void zest__add_report(zest_context context, zest_report_category category, int line_number, const char *file_name, const char *function_name, const char *entry, ...) {
    zest_text_t message = ZEST__ZERO_INIT(zest_text_t);
    va_list args;
    va_start(args, entry);
    zest_SetTextfv(context->device->allocator, &message, entry, args);
    va_end(args);
    zest_key report_hash = zest_Hash(message.str, zest_TextSize(&message), 0);
    if (zest_map_valid_key(context->device->reports, report_hash)) {
        zest_report_t *report = zest_map_at_key(context->device->reports, report_hash);
        report->count++;
        zest_FreeText(context->device->allocator, &message);
    } else {
        zest_report_t report = ZEST__ZERO_INIT(zest_report_t);
        report.count = 1;
        report.line_number = line_number;
        report.file_name = file_name;
        report.function_name = function_name;
        report.message = message;
        report.category = category;
        zest_map_insert_key(context->device->allocator, context->device->reports, report_hash, report);
    }
}

void zest__log_entry(zest_context context, const char *text, ...) {
    va_list args;
    va_start(args, text);
    zest_log_entry_t entry = ZEST__ZERO_INIT(zest_log_entry_t);
    entry.time = zest_Microsecs();
    zest__log_entry_v(entry.str, text, args);
    zest_vec_push(context->device->allocator, context->device->debug.frame_log, entry);
    va_end(args);
}

void zest__reset_frame_log(zest_context context, char *str, const char *entry, ...) {
    zest_vec_clear(context->device->debug.frame_log);
    context->device->debug.function_depth = 0;
}

void zest__add_line(zest_text_t *text, char current_char, zest_uint *position, zest_uint tabs) {
    ZEST_ASSERT(*position < zest_TextSize(text));
    text->str[(*position)++] = current_char;
    ZEST_ASSERT(*position < zest_TextSize(text));
    text->str[(*position)++] = '\n';
    for (int t = 0; t != tabs; ++t) {
        text->str[(*position)++] = '\t';
    }
}

zest_sampler_handle zest_CreateSampler(zest_context context, zest_sampler_info_t *info) {
    zest_sampler_handle sampler_handle = ZEST_STRUCT_LITERAL(zest_sampler_handle, zest__add_store_resource(zest_handle_type_samplers, context) );
    zest_sampler sampler = (zest_sampler)zest__get_store_resource(context, sampler_handle.value);
    sampler->magic = zest_INIT_MAGIC(zest_struct_type_sampler);
    sampler->create_info = *info;
    sampler->backend = (zest_sampler_backend)context->device->platform->new_sampler_backend(context);
	sampler_handle.context = context;
    sampler->handle = sampler_handle;

    if (context->device->platform->create_sampler(sampler)) {
        return sampler_handle;
    }

    zest__cleanup_sampler(sampler);
    return ZEST__ZERO_INIT(zest_sampler_handle);
}

zest_sampler_info_t zest_CreateSamplerInfo() {
    zest_sampler_info_t sampler_info = ZEST__ZERO_INIT(zest_sampler_info_t);
    sampler_info.mag_filter = zest_filter_linear;
    sampler_info.min_filter = zest_filter_linear;
    sampler_info.address_mode_u = zest_sampler_address_mode_clamp_to_edge;
    sampler_info.address_mode_v = zest_sampler_address_mode_clamp_to_edge;
    sampler_info.address_mode_w = zest_sampler_address_mode_clamp_to_edge;
    sampler_info.anisotropy_enable = ZEST_FALSE;
    sampler_info.max_anisotropy = 1.f;
    sampler_info.compare_enable = ZEST_FALSE;
    sampler_info.compare_op = zest_compare_op_always;
    sampler_info.mipmap_mode = zest_mipmap_mode_linear;
    sampler_info.border_color = zest_border_color_float_transparent_black;
    sampler_info.mip_lod_bias = 0.f;
    sampler_info.min_lod = 0.0f;
    sampler_info.max_lod = 14.0f;
    return sampler_info;
}

zest_sampler_handle zest_CreateSamplerForImage(zest_image_handle image_handle) {
    zest_image image = (zest_image)zest__get_store_resource_checked(image_handle.context, image_handle.value);
    zest_sampler_info_t sampler_info = zest_CreateSamplerInfo();
    sampler_info.max_lod = (float)image->info.mip_levels - 1.f;
    zest_sampler_handle sampler = zest_CreateSampler(image_handle.context, &sampler_info);
    return sampler;
}

void zest__prepare_standard_pipelines(zest_context context) {

    //Final Render Pipelines
    context->device->pipeline_templates.swap = zest_BeginPipelineTemplate(context, "pipeline_swap_chain");
    zest_pipeline_template swap = context->device->pipeline_templates.swap;
    zest_SetPipelinePushConstantRange(swap, sizeof(zest_vec2), zest_shader_vertex_stage);
    zest_SetPipelineBlend(swap, zest_PreMultiplyBlendStateForSwap());
    swap->no_vertex_input = ZEST_TRUE;
    zest_SetPipelineVertShader(swap, context->device->builtin_shaders.swap_vert);
    zest_SetPipelineFragShader(swap, context->device->builtin_shaders.swap_frag);
    swap->uniforms = 0;
    swap->flags = zest_pipeline_set_flag_is_render_target_pipeline;

    zest_ClearPipelineDescriptorLayouts(swap);
    swap->depth_stencil.depth_write_enable = ZEST_FALSE;
    swap->depth_stencil.depth_test_enable = ZEST_FALSE;

    swap->color_blend_attachment = zest_PreMultiplyBlendStateForSwap();

    ZEST_APPEND_LOG(context->device->log_path.str, "Final render pipeline");
}

// --End Renderer functions

// --General Helper Functions
zest_viewport_t zest_CreateViewport(float x, float y, float width, float height, float minDepth, float maxDepth) {
    zest_viewport_t viewport = ZEST__ZERO_INIT(zest_viewport_t);
    viewport.width = width;
    viewport.height = height;
    viewport.minDepth = minDepth;
    viewport.maxDepth = maxDepth;
    return viewport;
}

zest_scissor_rect_t zest_CreateRect2D(zest_uint width, zest_uint height, int offsetX, int offsetY) {
    zest_scissor_rect_t rect2D = ZEST__ZERO_INIT(zest_scissor_rect_t);
    rect2D.extent.width = width;
    rect2D.extent.height = height;
    rect2D.offset.x = offsetX;
    rect2D.offset.y = offsetY;
    return rect2D;
}

void zest__create_transient_buffer(zest_context context, zest_resource_node node) {
    node->storage_buffer = zest_CreateBuffer(context, node->buffer_desc.size, &node->buffer_desc.buffer_info);
}

zest_bool zest__create_transient_image(zest_context context, zest_resource_node resource) {
    zest_image image = &resource->image;
    image->magic = zest_INIT_MAGIC(zest_struct_type_image);
    image->backend = (zest_image_backend)context->device->platform->new_frame_graph_image_backend(context->frame_graph_allocator[context->current_fif], image, NULL);
    for (int i = 0; i != zest_max_global_binding_number; ++i) {
        image->bindless_index[i] = ZEST_INVALID;
    }
    image->info.flags |= zest_image_flag_transient;
    image->info.aspect_flags = zest__determine_aspect_flag(resource->image.info.format);
    image->info.mip_levels = resource->image.info.mip_levels > 0 ? resource->image.info.mip_levels : 1;
    if (ZEST__FLAGGED(image->info.flags, zest_image_flag_cubemap)) {
        ZEST_ASSERT(image->info.layer_count > 0 && image->info.layer_count % 6 == 0); // Cubemap must have layers in multiples of 6!
    }
    if (ZEST__FLAGGED(resource->image.info.flags, zest_image_flag_generate_mipmaps) && image->info.mip_levels == 1) {
        image->info.mip_levels = (zest_uint)floor(log2(ZEST__MAX(resource->image.info.extent.width, resource->image.info.extent.height))) + 1;
    }
    if (!context->device->platform->create_image(context, image, image->info.layer_count, zest_sample_count_1_bit, resource->image.info.flags)) {
		context->device->platform->cleanup_image_backend(image);
        return FALSE;
    }
	image->handle.context = context;
    image->info.layout = resource->image_layout = zest_image_layout_undefined;
    zest_image_view_type view_type = zest__get_image_view_type(image);
    image->default_view = context->device->platform->create_image_view(image, view_type, image->info.mip_levels, 0, 0, image->info.layer_count, context->frame_graph_allocator[context->current_fif]);
    image->default_view->handle.context = context;
    resource->view = image->default_view;
	resource->linked_layout = &image->info.layout;

    return ZEST_TRUE;
}

zest_index zest__next_fif(zest_context context) {
    return (context->current_fif + 1) % ZEST_MAX_FIF;
}

zest_create_info_t zest_CreateInfo() {
	zest_create_info_t create_info;
	create_info.title = "Zest Window";
	create_info.frame_graph_allocator_size = zloc__MEGABYTE(1);
	create_info.shader_path_prefix = "spv/";
	create_info.screen_width = 1280;
	create_info.screen_height = 768;
	create_info.screen_x = 0;
	create_info.screen_y = 50;
	create_info.virtual_width = 1280;
	create_info.virtual_height = 768;
	create_info.fence_wait_timeout_ms = 250;
	create_info.max_fence_timeout_ms = ZEST_SECONDS_IN_MILLISECONDS(10);
	create_info.color_format = zest_format_b8g8r8a8_unorm;
	create_info.flags = zest_init_flag_enable_vsync | zest_init_flag_cache_shaders;
	create_info.platform = zest_platform_vulkan;
	create_info.maximum_textures = 1024;
	create_info.store_allocator_size = zloc__MEGABYTE(2);
	create_info.bindless_sampler_count = 64;
	create_info.bindless_texture_2d_count = 256;
	create_info.bindless_texture_cube_count = 64;
	create_info.bindless_texture_array_count = 256;
	create_info.bindless_texture_3d_count = 64;
	create_info.bindless_storage_buffer_count = 256;
	create_info.bindless_storage_image_count = 256;
    return create_info;
}

zest_create_info_t zest_CreateInfoWithValidationLayers(zest_validation_flags flags) {
    zest_create_info_t create_info = zest_CreateInfo();
    ZEST__FLAG(create_info.flags, zest_init_flag_enable_validation_layers);
    if (flags & zest_validation_flag_enable_sync) {
        create_info.flags |= zest_init_flag_enable_validation_layers_with_sync;
    }
    if (flags & zest_validation_flag_best_practices) {
        create_info.flags |= zest_init_flag_enable_validation_layers_with_best_practices;
    }
    return create_info;
}

zest_file zest_ReadEntireFile(zest_context context, const char* file_name, zest_bool terminate) {
    char* buffer = 0;
    FILE* file = NULL;
    file = zest__open_file(file_name, "rb");
    if (!file) {
        return buffer;
    }

    // file invalid? fseek() fail?
    if (file == NULL || fseek(file, 0, SEEK_END)) {
        fclose(file);
        return buffer;
    }

    long length = ftell(file);
    rewind(file);
    // Did ftell() fail?  Is the length too long?
    if (length == -1 || (unsigned long)length >= SIZE_MAX) {
        fclose(file);
        return buffer;
    }

    if (terminate) {
        zest_vec_resize(context->device->allocator, buffer, (zest_uint)length + 1);
    }
    else {
        zest_vec_resize(context->device->allocator, buffer, (zest_uint)length);
    }
    if (buffer == 0 || fread(buffer, 1, length, file) != length) {
        zest_vec_free(context->device->allocator, buffer);
        fclose(file);
        return buffer;
    }
    if (terminate) {
        buffer[length] = '\0';
    }

    fclose(file);
    return buffer;
}

void zest_FreeFile(zest_context context, zest_file file) {
	zest_vec_free(context->device->allocator, file);
}
// --End General Helper Functions

// --frame graph functions
zest_frame_graph zest__new_frame_graph(zest_context context, const char *name) {
    zest_frame_graph frame_graph = (zest_frame_graph)zloc_LinearAllocation(context->frame_graph_allocator[context->current_fif], sizeof(zest_frame_graph_t));
    *frame_graph = ZEST__ZERO_INIT(zest_frame_graph_t);
    frame_graph->magic = zest_INIT_MAGIC(zest_struct_type_frame_graph);
    frame_graph->command_list.magic = zest_INIT_MAGIC(zest_struct_type_frame_graph_context);
	frame_graph->command_list.context = context;
    frame_graph->name = name;
    frame_graph->bindless_layout = context->device->global_bindless_set_layout;
    frame_graph->bindless_set = context->device->global_set;
    zest_bucket_array_init(context, &frame_graph->resources, zest_resource_node_t, 8);
    zest_bucket_array_init(context, &frame_graph->potential_passes, zest_pass_node_t, 8);
    return frame_graph;
}

zest_key zest__hash_frame_graph_cache_key(zest_frame_graph_cache_key_t *cache_key) {
    zest_key key = zest_Hash(&cache_key->auto_state, sizeof(zest_frame_graph_auto_state_t), ZEST_HASH_SEED);
    if (cache_key->user_state && cache_key->user_state_size) {
        key += zest_Hash(cache_key->user_state, cache_key->user_state_size, ZEST_HASH_SEED);
    }
    return key;
}

void zest__cache_frame_graph(zest_frame_graph frame_graph) {
    ZEST_ASSERT_HANDLE(frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
	zest_context context = zest__frame_graph_builder->context;
    if (!frame_graph->cache_key) return;    //Only cache if there's a key
    if (frame_graph->error_status) return;  //Don't cache a frame graph that had errors
    zest_cached_frame_graph_t new_cached_graph = {
        zloc_PromoteLinearBlock(context->device->allocator, context->frame_graph_allocator[context->current_fif], context->frame_graph_allocator[context->current_fif]->current_offset),
        frame_graph
    };
    ZEST__FLAG(frame_graph->flags, zest_frame_graph_is_cached);
	void *frame_graph_linear_memory = ZEST__ALLOCATE(context->device->allocator, zloc__MEGABYTE(1));
	context->frame_graph_allocator[context->current_fif] = zloc_InitialiseLinearAllocator(frame_graph_linear_memory, zloc__MEGABYTE(1));
    if (zest_map_valid_key(context->device->cached_frame_graphs, frame_graph->cache_key)) {
        zest_cached_frame_graph_t *cached_graph = zest_map_at_key(context->device->cached_frame_graphs, frame_graph->cache_key);
        ZEST__FREE(context->device->allocator, cached_graph->memory);
        *cached_graph = new_cached_graph;
    } else {
        zest_map_insert_key(context->device->allocator, context->device->cached_frame_graphs, frame_graph->cache_key, new_cached_graph);
    }
}

zest_image_view zest__swapchain_resource_provider(zest_context context, zest_resource_node resource) {
    return resource->swapchain->views[resource->swapchain->current_image_frame];
}

zest_buffer zest__instance_layer_resource_provider(zest_context context, zest_resource_node resource) {
    zest_layer layer = (zest_layer)resource->user_data;
    layer->vertex_buffer_node = resource;
    zest__end_instance_instructions(layer); //Make sure the staging buffer memory in use is up to date
    if (ZEST__NOT_FLAGGED(layer->flags, zest_layer_flag_manual_fif)) {
		zest_size layer_size = layer->memory_refs[layer->fif].instance_count * layer->instance_struct_size;
        if (layer_size == 0) return NULL;
        layer->fif = context->current_fif;
        layer->dirty[context->current_fif] = 1;
        zest_buffer_resource_info_t buffer_desc = ZEST__ZERO_INIT(zest_buffer_resource_info_t);
        buffer_desc.size = layer_size;
        buffer_desc.usage_hints = zest_resource_usage_hint_vertex_buffer;
    } else {
        zest_uint fif = ZEST__FLAGGED(layer->flags, zest_layer_flag_use_prev_fif) ? layer->prev_fif : layer->fif;
        resource->bindless_index[0] = layer->memory_refs[fif].device_vertex_data->array_index;
        return layer->memory_refs[fif].device_vertex_data;
    }
    return NULL;
}

zest_frame_graph zest_GetCachedFrameGraph(zest_context context, zest_frame_graph_cache_key_t *cache_key) {
	zest_key key = zest__hash_frame_graph_cache_key(cache_key);
    if (zest_map_valid_key(context->device->cached_frame_graphs, key)) {
        zest_cached_frame_graph_t *cached_graph = zest_map_at_key(context->device->cached_frame_graphs, key);
        return cached_graph->frame_graph;
    }
    return NULL;
}

zest_frame_graph_cache_key_t zest_InitialiseCacheKey(zest_context context, const void *user_state, zest_size user_state_size) {
    zest_frame_graph_cache_key_t key = ZEST__ZERO_INIT(zest_frame_graph_cache_key_t);
    key.auto_state.render_format = context->swapchain->format;
    key.auto_state.render_width = (zest_uint)context->swapchain->size.width;
    key.auto_state.render_height = (zest_uint)context->swapchain->size.height;
    key.user_state = user_state;
    key.user_state_size = user_state_size;
    return key;
}

void zest_QueueFrameGraphForExecution(zest_context context, zest_frame_graph frame_graph) {
	ZEST_ASSERT_HANDLE(context);	    //Not a valid context handle
	ZEST_ASSERT_HANDLE(frame_graph);	//Not a valid frame graph handle
	ZEST_ASSERT(context == frame_graph->command_list.context);	//The context must equal the context that was used for the frame graph.
	if (frame_graph->error_status == zest_fgs_success) {
		zest_vec_linear_push(context->frame_graph_allocator[context->current_fif], context->frame_graphs, frame_graph);
	} else {
		ZEST__REPORT(context, zest_report_cannot_execute, zest_message_cannot_queue_for_execution, frame_graph->name);
	}
}

zest_bool zest_BeginFrameGraph(zest_context context, const char *name, zest_frame_graph_cache_key_t *cache_key) {
	ZEST_ASSERT(ZEST__NOT_FLAGGED(context->flags, zest_context_flag_building_frame_graph));  //frame graph already being built. You cannot build a frame graph within another begin frame graph process.
	
	ZEST_ASSERT(zest__frame_graph_builder == NULL);		//The thread local frame graph builder must be null
														//If it's not null then is there already a frame graph being set up, and 
														//did you call EndFrameGraph?

	zest__frame_graph_builder = (zest_frame_graph_builder)zloc_LinearAllocation(context->frame_graph_allocator[context->current_fif], sizeof(zest_frame_graph_builder_t));
	zest__frame_graph_builder->context = context;
	zest__frame_graph_builder->current_pass = 0;

    zest_key key = 0;
    if (cache_key) {
        key = zest__hash_frame_graph_cache_key(cache_key);
    }
    zest_frame_graph frame_graph = zest__new_frame_graph(context, name);
    frame_graph->cache_key = key;

    frame_graph->semaphores = context->device->platform->get_frame_graph_semaphores(context, name);
    frame_graph->command_list.backend = (zest_command_list_backend)context->device->platform->new_frame_graph_context_backend(context);

	ZEST__UNFLAG(frame_graph->flags, zest_frame_graph_expecting_swap_chain_usage);
	ZEST__FLAG(context->flags, zest_context_flag_building_frame_graph);
	zest__frame_graph_builder->frame_graph = frame_graph;
    return ZEST_TRUE;
}

zest_bool zest__is_stage_compatible_with_qfi(zest_pipeline_stage_flags stages_to_check, zest_device_queue_type queue_family_capabilities) {
    if (stages_to_check == zest_pipeline_stage_none) { // Or just == 0
        return ZEST_TRUE; // No stages specified is trivially compatible
    }

    zest_pipeline_stage_flags current_stages_to_evaluate = stages_to_check;

    // Iterate through each individual bit set in stages_to_check
    while (current_stages_to_evaluate != 0) {
        // Get the lowest set bit
        zest_pipeline_stage_flags single_stage_bit = current_stages_to_evaluate & (~current_stages_to_evaluate + 1);
        // Remove this bit for the next iteration
        current_stages_to_evaluate &= ~single_stage_bit;

        bool stage_is_compatible = ZEST_FALSE;
        switch (single_stage_bit) {
            // Stages allowed on ANY queue type that supports any command submission
        case zest_pipeline_stage_top_of_pipe_bit:
        case zest_pipeline_stage_bottom_of_pipe_bit:
        case zest_pipeline_stage_host_bit: // Host access can always be synchronized against
        case zest_pipeline_stage_all_commands_bit: // Valid, but its scope is limited by queue caps
            stage_is_compatible = ZEST_TRUE;
            break;

            // Stages requiring GRAPHICS_BIT capability
        case zest_pipeline_stage_vertex_input_bit:
        case zest_pipeline_stage_vertex_shader_bit:
        case zest_pipeline_stage_tessellation_control_shader_bit:
        case zest_pipeline_stage_tessellation_evaluation_shader_bit:
        case zest_pipeline_stage_geometry_shader_bit:
        case zest_pipeline_stage_fragment_shader_bit:
        case zest_pipeline_stage_early_fragment_tests_bit:
        case zest_pipeline_stage_late_fragment_tests_bit:
        case zest_pipeline_stage_color_attachment_output_bit:
        case zest_pipeline_stage_all_graphics_bit:
        //Can add more extensions here if needed
            if (queue_family_capabilities & zest_queue_graphics) {
                stage_is_compatible = ZEST_TRUE;
            }
            break;

		// Stage requiring COMPUTE_BIT capability
        case zest_pipeline_stage_compute_shader_bit:
            if (queue_family_capabilities & zest_queue_compute) {
                stage_is_compatible = ZEST_TRUE;
            }
            break;

		// Stage DRAW_INDIRECT can be used by Graphics or Compute
        case zest_pipeline_stage_draw_indirect_bit:
            if (queue_family_capabilities & (zest_queue_graphics | zest_queue_compute)) {
                stage_is_compatible = ZEST_TRUE;
            }
            break;

		// Stage requiring TRANSFER_BIT capability (also often implied by Graphics/Compute)
        case zest_pipeline_stage_transfer_bit:
            if (queue_family_capabilities & (zest_queue_graphics | zest_queue_compute | zest_queue_transfer)) {
                stage_is_compatible = ZEST_TRUE;
            }
            break;

        default:
            stage_is_compatible = ZEST_TRUE; 
            break;
        }

        if (!stage_is_compatible) {
            return ZEST_FALSE; 
        }
    }

    return ZEST_TRUE; 
}

void zest__free_transient_resource(zest_resource_node resource) {
    if (resource->type & zest_resource_type_buffer) {
        zest_FreeBuffer(resource->storage_buffer);
        resource->storage_buffer = 0;
    } else if (resource->type & zest_resource_type_is_image_or_depth) {
        zest_FreeBuffer(resource->image.buffer);
        resource->image.buffer = 0;
    }
}

zest_bool zest__create_transient_resource(zest_context context, zest_resource_node resource) {
    if (resource->type & zest_resource_type_is_image) {
        if (!zest__create_transient_image(context, resource)) {
            return ZEST_FALSE;
        }

        resource->image_layout = zest_image_layout_undefined;
        resource->access_mask = 0;
        resource->last_stage_mask = zest_pipeline_stage_top_of_pipe_bit;
		zest_frame_graph frame_graph = zest__frame_graph_builder->frame_graph;
		zest_vec_linear_push(context->frame_graph_allocator[context->current_fif], frame_graph->deferred_image_destruction, resource);
    } else if (ZEST__FLAGGED(resource->type, zest_resource_type_buffer) && resource->storage_buffer == NULL) {
        zest__create_transient_buffer(context, resource);
        if (!resource->storage_buffer) {
            return ZEST_FALSE;
        }
        resource->access_mask = 0;
        resource->last_stage_mask = zest_pipeline_stage_top_of_pipe_bit;
    }
    return ZEST_TRUE;
}

#ifdef ZEST_TEST_MODE
void zest__add_image_barrier(zest_resource_node resource, zest_execution_barriers_t *barriers, zest_bool acquire, 
        zest_access_flags src_access, zest_access_flags dst_access, zest_image_layout old_layout, zest_image_layout new_layout, 
        zest_uint src_family, zest_uint dst_family, zest_pipeline_stage_flags src_stage, zest_pipeline_stage_flags dst_stage) {
	zest_context context = resource->frame_graph->command_list.context;
	zest_image_barrier_t image_barrier = {
		src_access, dst_access,
		old_layout, new_layout,
		src_family,
		dst_family
	};
    if (acquire) {
        zest_vec_linear_push(context->frame_graph_allocator[context->current_fif], barriers->acquire_image_barriers, image_barrier);
		barriers->overall_src_stage_mask_for_acquire_barriers |= src_stage;
		barriers->overall_dst_stage_mask_for_acquire_barriers |= dst_stage;
    } else {
        zest_vec_linear_push(context->frame_graph_allocator[context->current_fif], barriers->release_image_barriers, image_barrier);
		barriers->overall_src_stage_mask_for_release_barriers |= src_stage;
		barriers->overall_dst_stage_mask_for_release_barriers |= dst_stage;
    }
}

void zest__add_buffer_barrier(zest_resource_node resource, zest_execution_barriers_t *barriers, zest_bool acquire, zest_access_flags src_access, zest_access_flags dst_access,
    zest_uint src_family, zest_uint dst_family, zest_pipeline_stage_flags src_stage, zest_pipeline_stage_flags dst_stage) {
	zest_context context = resource->frame_graph->command_list.context;
	zest_buffer_barrier_t buffer_barrier {
		src_access, dst_access,
		src_family, dst_family
	};
    if (acquire) {
        zest_vec_linear_push(context->frame_graph_allocator[context->current_fif], barriers->acquire_buffer_barriers, buffer_barrier);
        barriers->overall_src_stage_mask_for_acquire_barriers |= src_stage;
        barriers->overall_dst_stage_mask_for_acquire_barriers |= dst_stage;
    } else {
        zest_vec_linear_push(context->frame_graph_allocator[context->current_fif], barriers->release_buffer_barriers, buffer_barrier);
        barriers->overall_src_stage_mask_for_release_barriers |= src_stage;
        barriers->overall_dst_stage_mask_for_release_barriers |= dst_stage;
    }
}
#endif

void zest__add_image_barriers(zest_frame_graph frame_graph, zloc_linear_allocator_t *allocator, zest_resource_node resource, zest_execution_barriers_t *barriers, 
                              zest_resource_state_t *current_state, zest_resource_state_t *prev_state, zest_resource_state_t *next_state) {
	zest_resource_usage_t *current_usage = &current_state->usage;
	zest_image_layout current_usage_layout = current_usage->image_layout;
	zest_context context = zest__frame_graph_builder->context;
    if (!prev_state) {
        //This is the first state of the resource
        //If there's no previous state then we need to see if a barrier is needed to transition from the resource
        //start state. We put this in the acquire barrier as it needs to be put in place before the pass is executed.
        zest_uint src_queue_family_index = resource->current_queue_family_index;
        zest_uint dst_queue_family_index = ZEST_QUEUE_FAMILY_IGNORED;
        if (src_queue_family_index == ZEST_QUEUE_FAMILY_IGNORED) {
            if (resource->image_layout != current_usage_layout ||
                (resource->access_mask & zest_access_write_bits_general) &&
                (current_usage->access_mask & zest_access_read_bits_general)) {
                context->device->platform->add_frame_graph_image_barrier(resource, barriers, true,
                    resource->access_mask, current_usage->access_mask,
                    resource->image_layout, current_usage_layout,
                    src_queue_family_index, dst_queue_family_index,
                    resource->last_stage_mask, current_usage->stage_mask);
				#ifdef ZEST_TEST_MODE
                zest__add_image_barrier(resource, barriers, true,
                    resource->access_mask, current_usage->access_mask,
                    resource->image_layout, current_usage_layout,
                    src_queue_family_index, dst_queue_family_index,
                    resource->last_stage_mask, current_usage->stage_mask);
				#endif
            }
        } else {
            //This resource already belongs to a queue which means that it's an imported resource
            //If the frame graph is on the graphics queue only then there's no need to acquire from a prior release.
            ZEST_ASSERT(ZEST__FLAGGED(resource->flags, zest_resource_node_flag_imported));
            dst_queue_family_index = current_state->queue_family_index;
            if (src_queue_family_index != ZEST_QUEUE_FAMILY_IGNORED) {
                context->device->platform->add_frame_graph_image_barrier(resource, barriers, true,
                    resource->access_mask, current_usage->access_mask,
                    resource->image_layout, current_usage_layout,
                    src_queue_family_index, dst_queue_family_index,
                    resource->last_stage_mask, current_usage->stage_mask);
				#ifdef ZEST_TEST_MODE
                zest__add_image_barrier(resource, barriers, true,
                    resource->access_mask, current_usage->access_mask,
                    resource->image_layout, current_usage_layout,
                    src_queue_family_index, dst_queue_family_index,
                    resource->last_stage_mask, current_usage->stage_mask);
				#endif
            }
        }
        bool needs_releasing = false;
    } else {
        //There is a previous state, do we need to acquire the resource from a different queue?
        zest_uint src_queue_family_index = ZEST_QUEUE_FAMILY_IGNORED;
        zest_uint dst_queue_family_index = ZEST_QUEUE_FAMILY_IGNORED;
        bool needs_acquiring = false;
        if (prev_state && (prev_state->queue_family_index != current_state->queue_family_index || prev_state->was_released)) {
            //The next state is in a different queue so the resource needs to be released to that queue
            src_queue_family_index = prev_state->queue_family_index;
            dst_queue_family_index = current_state->queue_family_index;
            needs_acquiring = true;
        }
        if (needs_acquiring) {
            zest_resource_usage_t *prev_usage = &prev_state->usage;
            //Acquire the resource. No transitioning is done here, acquire only if needed
            context->device->platform->add_frame_graph_image_barrier(resource, barriers, true,
                zest_access_none, current_usage->access_mask,
                prev_usage->image_layout, current_usage_layout,
                src_queue_family_index, dst_queue_family_index,
                zest_pipeline_stage_top_of_pipe_bit, current_usage->stage_mask);
			#ifdef ZEST_TEST_MODE
            zest__add_image_barrier(resource, barriers, true,
                zest_access_none, current_usage->access_mask,
                prev_usage->image_layout, current_usage_layout,
                src_queue_family_index, dst_queue_family_index,
                zest_pipeline_stage_top_of_pipe_bit, current_usage->stage_mask);
			#endif
            //Make sure that the batch that this resource is in (at this point) has the correct wait
            //stage to wait on.
            zest_submission_batch_t *batch = zest__get_submission_batch(current_state->submission_id);
            batch->queue_wait_stages |= zest_pipeline_stage_top_of_pipe_bit;
        }
    }
    if (next_state) {
        //If the resource has a state after this one then we can check if a barrier is needed to transition and/or
        //release to another queue family
        zest_uint src_queue_family_index = current_state->queue_family_index;
        zest_uint dst_queue_family_index = next_state->queue_family_index;
        zest_resource_usage_t *next_usage = &next_state->usage;
		zest_image_layout next_usage_layout = next_usage->image_layout;
        bool needs_releasing = false;
        if (next_state && next_state->queue_family_index != current_state->queue_family_index) {
            //The next state is in a different queue so the resource needs to be released to that queue
            src_queue_family_index = current_state->queue_family_index;
            dst_queue_family_index = next_state->queue_family_index;
            needs_releasing = true;
        }
        if ((needs_releasing || current_usage->image_layout != next_usage->image_layout ||
            (current_usage->access_mask & zest_access_write_bits_general) &&
            (next_usage->access_mask & zest_access_read_bits_general))) {
            zest_pipeline_stage_flags dst_stage = zest_pipeline_stage_bottom_of_pipe_bit;
            if (needs_releasing) {
                current_state->was_released = true;
            } else {
                dst_stage = next_state->usage.stage_mask;
                current_state->was_released = false;
            }
            context->device->platform->add_frame_graph_image_barrier(resource, barriers, false,
                current_usage->access_mask, zest_access_none,
                current_usage->image_layout, next_usage_layout,
                src_queue_family_index, dst_queue_family_index,
                current_usage->stage_mask, dst_stage);
			#ifdef ZEST_TEST_MODE
            zest__add_image_barrier(resource, barriers, false,
                current_usage->access_mask, zest_access_none,
                current_usage->image_layout, next_usage_layout,
                src_queue_family_index, dst_queue_family_index,
                current_usage->stage_mask, dst_stage);
			#endif
        }
    } else if (resource->flags & zest_resource_node_flag_imported
        && current_state->queue_family_index != context->device->transfer_queue_family_index) {
        //Maybe we add something for images here if it's needed.
    }
}

zest_bool zest__detect_cyclic_recursion(zest_frame_graph frame_graph, zest_pass_node pass_node) {
    pass_node->visit_state = zest_pass_node_visiting;
    zest_map_foreach(i, pass_node->outputs) {
        zest_resource_usage_t *output_usage = &pass_node->outputs.data[i];
        zest_resource_node output_resource = output_usage->resource_node;
        zest_bucket_array_foreach(p_idx, frame_graph->potential_passes) {
            zest_pass_node dependent_pass = zest_bucket_array_get(&frame_graph->potential_passes, zest_pass_node_t, p_idx);
            if (zest_map_valid_name(dependent_pass->inputs, output_resource->name)) {
                if (dependent_pass == pass_node) {
                    continue;
                }
                if (dependent_pass->visit_state == zest_pass_node_visiting) {
                    return ZEST_TRUE; // Signal that a cycle was found
                }
                if (dependent_pass->visit_state == zest_pass_node_unvisited) {
                    if (zest__detect_cyclic_recursion(frame_graph, dependent_pass)) {
                        return ZEST_TRUE; // Propagate the cycle detection signal up
                    }
                }
            }
        }
    }
    pass_node->visit_state = zest_pass_node_visited;
    return ZEST_FALSE; // No cycle found from this pass
}

/*
frame graph compiler index:

Main compile phases:
    - Initial cull of passes with no outputs or execution callbacks
    - Put non culled passes into the ungrouped_passes list
    - Calculate dependencies on these ungrouped passes to build the graph
    - Build an ordered execution list from this
    - Group passes together that can be based on shared output, input dependencies and queue families
    - Build "resource journeys" for each resource for future barrier creation.
    - Calculate the lifetimes of resources for transient memory aliasing
    - Group these passes in to submission batches based on queue families
    - Create semaphores for the submission batches
    - Create lists ready for execution for barrier and transient buffer/image craeation during the execution stage
    - Process exexution list and create or fetch cached render passes.

[Check_unused_resources_and_passes]
[Set_producers_and_consumers]
[Set_adjacency_list]
[Create_execution_waves]
[Create_command_batches]
[Resource_journeys]
[Calculate_lifetime_of_resources]
[Create_semaphores]
[Plan_transient_buffers]
[Plan_resource_barriers]
[Create_memory_barriers_for_inputs]
[Create_memory_barriers_for_outputs]
[Process_compiled_execution_order]
[Prepare_render_passes]
*/
zest_frame_graph zest__compile_frame_graph() {

    zest_frame_graph frame_graph = zest__frame_graph_builder->frame_graph;
	zest_context context = zest__frame_graph_builder->context;
    ZEST_ASSERT_HANDLE(frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen

    zloc_linear_allocator_t *allocator = context->frame_graph_allocator[context->current_fif];

    zest_bucket_array_foreach(i, frame_graph->potential_passes) {
        zest_pass_node pass_node = zest_bucket_array_get(&frame_graph->potential_passes, zest_pass_node_t, i);
        if (pass_node->visit_state == zest_pass_node_unvisited) {
            if (zest__detect_cyclic_recursion(frame_graph, pass_node)) {
                ZEST__REPORT(context, zest_report_cyclic_dependency, zest_message_cyclic_dependency, frame_graph->name, pass_node->name);
                ZEST__FLAG(frame_graph->error_status, zest_fgs_cyclic_dependency);
                return frame_graph;
            }
        }
    }

    //Check_unused_resources_and_passes and cull them if necessary
    bool a_pass_was_culled = 0;
    zest_bucket_array_foreach(i, frame_graph->potential_passes) {
        zest_pass_node pass_node = zest_bucket_array_get(&frame_graph->potential_passes, zest_pass_node_t, i);
        if (zest_map_size(pass_node->outputs) == 0 || pass_node->execution_callback.callback == 0) {
			//Cull passes that have no output and/or no execution callback and reduce any input resource reference counts 
            zest_map_foreach(j, pass_node->inputs) {
                pass_node->inputs.data[j].resource_node->reference_count--;
            }
            ZEST__FLAG(pass_node->flags, zest_pass_flag_culled);
            a_pass_was_culled = true;
            if (zest_map_size(pass_node->outputs) == 0) {
                ZEST__REPORT(context, zest_report_pass_culled, zest_message_pass_culled, pass_node->name);
            } else {
                ZEST__REPORT(context, zest_report_pass_culled, zest_message_pass_culled_no_work, pass_node->name);
            }
        } else {
            zest_uint reference_counts = 0;
            zest_map_foreach(output_index, pass_node->outputs) {
                zest_resource_node resource = pass_node->outputs.data[output_index].resource_node;
                if (ZEST__FLAGGED(resource->flags, zest_resource_node_flag_essential_output)) {
					reference_counts += 1;
                    continue;
                }
				reference_counts += resource->reference_count;
            }
            if (reference_counts == 0) {
                //Outputs are not consumed by any other pass so this pass can be culled
				zest_map_foreach(j, pass_node->inputs) {
					pass_node->inputs.data[j].resource_node->reference_count--;
				}
                ZEST__FLAG(pass_node->flags, zest_pass_flag_culled);
				a_pass_was_culled = true;
				ZEST__REPORT(context, zest_report_pass_culled, zest_message_pass_culled_not_consumed, pass_node->name);
            }
        }
    }

    //Keep iterating and culling passes whose output resources have 0 reference counts and keep reducing input resource reference counts
    while (a_pass_was_culled) {
        a_pass_was_culled = false;
		zest_bucket_array_foreach(i, frame_graph->potential_passes) {
			zest_pass_node pass_node = zest_bucket_array_get(&frame_graph->potential_passes, zest_pass_node_t, i);
            if (ZEST__NOT_FLAGGED(pass_node->flags, zest_pass_flag_culled) && ZEST__NOT_FLAGGED(pass_node->flags, zest_pass_flag_do_not_cull)) {
                bool all_outputs_non_referenced = true;
                zest_map_foreach(j, pass_node->outputs) {
                    if (pass_node->outputs.data[j].resource_node->reference_count > 0) {
                        all_outputs_non_referenced = false;
                        break;
                    }
                }
                if (all_outputs_non_referenced) {
                    ZEST__FLAG(pass_node->flags, zest_pass_flag_culled);
                    a_pass_was_culled = true;
                    zest_map_foreach(j, pass_node->inputs) {
						ZEST__MAYBE_REPORT(context, pass_node->inputs.data[j].resource_node->reference_count == 0, zest_report_invalid_reference_counts, zest_message_pass_culled_not_consumed, frame_graph->name);
                        frame_graph->error_status |= zest_fgs_critical_error;
                        return frame_graph;
                        pass_node->inputs.data[j].resource_node->reference_count--;
                    }
					ZEST__REPORT(context, zest_report_pass_culled, zest_message_pass_culled_not_consumed, pass_node->name);
                }
            }
        }
    }

    bool has_execution_callback = false;

    //Now group the potential passes in to final passes by ignoring any culled passes and grouping together passes that
    //have the same output. We need to be careful here though to make sure that even though 2 passes might share the same output
    //they might have conflicting dependencies. Output keys are generated by hashing the usage options for the resource.
	zest_bucket_array_foreach(i, frame_graph->potential_passes) {
		zest_pass_node pass_node = zest_bucket_array_get(&frame_graph->potential_passes, zest_pass_node_t, i);
        //Ignore culled passes
        if (ZEST__NOT_FLAGGED(pass_node->flags, zest_pass_flag_culled)) {
            //If no entry for the current pass output key exists, create one.
            if (!zest_map_valid_key(frame_graph->final_passes, pass_node->output_key)) {
                zest_pass_group_t pass_group = ZEST__ZERO_INIT(zest_pass_group_t);
                pass_group.execution_details.barriers.backend = (zest_execution_barriers_backend)context->device->platform->new_execution_barriers_backend(allocator);
                pass_group.queue_info = pass_node->queue_info;
                zest_map_foreach(output_index, pass_node->outputs) {
                    zest_hash_pair pair = pass_node->outputs.map[output_index];
                    zest_resource_usage_t *usage = &pass_node->outputs.data[pair.index];
                    if (!ZEST_VALID_HANDLE(usage->resource_node)) {
                        ZEST__REPORT(context, zest_report_invalid_resource, zest_message_usage_has_no_resource, frame_graph->name);
						frame_graph->error_status |= zest_fgs_critical_error;
						return frame_graph;
                    }
					if (ZEST__FLAGGED(usage->resource_node->type, zest_resource_type_swap_chain_image)) {
						ZEST__FLAG(pass_group.flags, zest_pass_flag_outputs_to_swapchain);
					}
                    if (usage->resource_node->reference_count > 0 || ZEST__FLAGGED(usage->resource_node->flags, zest_resource_node_flag_essential_output)) {
                        zest_map_insert_linear_key(allocator, pass_group.outputs, pass_node->outputs.map[output_index].key, *usage);
                    }
                }
                zest_map_foreach(input_index, pass_node->inputs) {
                    zest_hash_pair pair = pass_node->inputs.map[input_index];
                    zest_resource_usage_t *usage = &pass_node->inputs.data[pair.index];
                    if (!ZEST_VALID_HANDLE(usage->resource_node)) {
                        ZEST__REPORT(context, zest_report_invalid_resource, zest_message_usage_has_no_resource, frame_graph->name);
						frame_graph->error_status |= zest_fgs_critical_error;
						return frame_graph;
                    }
                    zest_map_insert_linear_key(allocator, pass_group.inputs, pair.key, *usage);
                }
                if (pass_node->execution_callback.callback) {
                    has_execution_callback = true;
                }
                ZEST__FLAG(pass_group.flags, pass_node->flags);
                zest_vec_linear_push(allocator, pass_group.passes, pass_node);
                zest_map_insert_linear_key(allocator, frame_graph->final_passes, pass_node->output_key, pass_group);
            } else {
                zest_pass_group_t *pass_group = zest_map_at_key(frame_graph->final_passes, pass_node->output_key);
				if (pass_group->queue_info.queue_family_index != pass_node->queue_info.queue_family_index ||
                    pass_group->queue_info.timeline_wait_stage != pass_node->queue_info.timeline_wait_stage) {
					ZEST__REPORT(context, zest_report_invalid_pass, zest_message_multiple_swapchain_usage, frame_graph->name);
					frame_graph->error_status |= zest_fgs_critical_error;
					return frame_graph;
				}
                if (pass_node->execution_callback.callback) {
                    has_execution_callback = true;
                }
                ZEST__FLAG(pass_group->flags, pass_node->flags);
                zest_vec_linear_push(allocator, pass_group->passes, pass_node);
                zest_map_foreach(input_index, pass_node->inputs) {
                    zest_hash_pair pair = pass_node->inputs.map[input_index];
                    zest_resource_usage_t *usage = &pass_node->inputs.data[pair.index];
                    if (!ZEST_VALID_HANDLE(usage->resource_node)) {
						ZEST__REPORT(context, zest_report_invalid_resource, zest_message_usage_has_no_resource, frame_graph->name);
						frame_graph->error_status |= zest_fgs_critical_error;
						return frame_graph;
                    }
                    zest_map_insert_linear_key(allocator, pass_group->inputs, pair.key, *usage);
                }
            }
        } else {
            frame_graph->culled_passes_count++;
        }
    }

    zest_uint potential_passes = zest_bucket_array_size(&frame_graph->potential_passes);
    zest_uint final_passes = zest_map_size(frame_graph->final_passes);

    if (!has_execution_callback) {
        zest__set_rg_error_status(frame_graph, zest_fgs_no_work_to_do);
        return frame_graph;
    }

    zest_pass_adjacency_list_t *adjacency_list = 0;
    zest_uint *dependency_count = 0;
    zest_vec_linear_resize(allocator, adjacency_list, zest_map_size(frame_graph->final_passes));
    zest_vec_linear_resize(allocator, dependency_count, zest_map_size(frame_graph->final_passes));

    // Set_producers_and_consumers:
    // For each output in a pass, we set it's producer_pass_idx - the pass that writes the output
    // and for each input in a pass we add all of the comuser_pass_idx's that read the inputs
    zest_map_foreach(i, frame_graph->final_passes) {
        zest_pass_group_t *pass_node = &frame_graph->final_passes.data[i];
        dependency_count[i] = 0; // Initialize in-degree
        zest_pass_adjacency_list_t adj_list = ZEST__ZERO_INIT(zest_pass_adjacency_list_t);
        adjacency_list[i] = adj_list;

        switch (pass_node->queue_info.queue_type) {
        case zest_queue_graphics:
            pass_node->queue_info.queue = &context->device->graphics_queue;
            pass_node->queue_info.queue_family_index = context->device->graphics_queue_family_index;
            break;
        case zest_queue_compute:
            pass_node->queue_info.queue = &context->device->compute_queue;
            pass_node->queue_info.queue_family_index = context->device->compute_queue_family_index;
            break;
        case zest_queue_transfer:
            pass_node->queue_info.queue = &context->device->transfer_queue;
            pass_node->queue_info.queue_family_index = context->device->transfer_queue_family_index;
            break;
        }

		pass_node->compiled_queue_info = pass_node->queue_info;

        zest_map_foreach(j, pass_node->inputs) {
            zest_resource_usage_t *input_usage = &pass_node->inputs.data[j];
            zest_resource_node resource = input_usage->resource_node;
            zest_resource_versions_t *versions = zest_map_at_key(frame_graph->resource_versions, resource->original_id);
            ZEST_ASSERT(versions);  //Bug, this shouldn't be possible, all resources should have a version

            zest_resource_node latest_version = zest_vec_back(versions->resources);
            if (!zest_map_valid_name(pass_node->inputs, latest_version->name)) {
                latest_version->reference_count++;
            }
            input_usage->resource_node = latest_version;

            zest_map_linear_insert(context->frame_graph_allocator[context->current_fif], pass_node->inputs, latest_version->name, *input_usage);
            zest_vec_linear_push(allocator, resource->consumer_pass_indices, i); // pass 'i' consumes this
        }

        zest_map_foreach(j, pass_node->outputs) {
            zest_resource_usage_t *output_usage = &pass_node->outputs.data[j];
            zest_resource_node resource = output_usage->resource_node;
            zest_resource_versions_t *versions = zest__maybe_add_resource_version(resource);
            zest_resource_node latest_version = zest_vec_back(versions->resources);
            if (latest_version->id != resource->id) {
                if (resource->type == zest_resource_type_swap_chain_image) {
                    ZEST__REPORT(context, zest_report_multiple_swapchains, zest_message_multiple_swapchain_usage, frame_graph->name);
                    frame_graph->error_status |= zest_fgs_critical_error;
					return frame_graph;
                }
			    //Add the versioned alias to the outputs instead
                output_usage->resource_node = latest_version;
                zest_map_linear_insert(context->frame_graph_allocator[context->current_fif], pass_node->outputs, latest_version->name, *output_usage);
                //Check if the user already added this as input:
                if (!zest_map_valid_name(pass_node->inputs, resource->name)) {
                    //If not then add the resource as input for correct dependency chain with default usages
                    zest_resource_usage_t input_usage = ZEST__ZERO_INIT(zest_resource_usage_t);
                    switch (pass_node->queue_info.queue_type) {
                    case zest_queue_graphics:
                        input_usage = zest__configure_image_usage(resource, zest_purpose_color_attachment_read, resource->image.info.format, output_usage->load_op, output_usage->stencil_load_op, output_usage->stage_mask);
                        break;
                    case zest_queue_compute:
                    case zest_queue_transfer:
                        input_usage = zest__configure_image_usage(resource, zest_purpose_storage_image_read, resource->image.info.format, output_usage->load_op, output_usage->stencil_load_op, output_usage->stage_mask);
                        break;
                    }
                    input_usage.store_op = output_usage->store_op;
                    input_usage.stencil_store_op = output_usage->stencil_store_op;
                    input_usage.clear_value = output_usage->clear_value;
                    input_usage.purpose = output_usage->purpose;
                    resource->reference_count++;
                    zest_map_linear_insert(context->frame_graph_allocator[context->current_fif], pass_node->inputs, resource->name, input_usage);
                }
            }

            if (output_usage->resource_node->producer_pass_idx != -1 && output_usage->resource_node->producer_pass_idx != i) {
				ZEST__REPORT(context, zest_report_multiple_swapchains, zest_message_resource_added_as_ouput_more_than_once, frame_graph->name);
				frame_graph->error_status |= zest_fgs_critical_error;
				return frame_graph;
            }
            output_usage->resource_node->producer_pass_idx = i;
        }

    }

    // Set_adjacency_list
    zest_map_foreach(consumer_idx, frame_graph->final_passes) {
        zest_pass_group_t *consumer_pass = &frame_graph->final_passes.data[consumer_idx];
        zest_map_foreach(j, consumer_pass->inputs) {
            zest_resource_usage_t *input_usage = &consumer_pass->inputs.data[j];
            zest_resource_node resource = input_usage->resource_node;

            int producer_idx = resource->producer_pass_idx;
            if (producer_idx != -1 && producer_idx != consumer_idx) {
                // Dependency: producer_idx ---> consumer_idx
                // Add consumer_idx to the list of passes that producer_idx must precede
                // Ensure no duplicates if a pass reads the same produced resource multiple times in its input list
                zest_bool already_linked = ZEST_FALSE;
                zest_vec_foreach(k_adj, adjacency_list[producer_idx].pass_indices) {
                    if (adjacency_list[producer_idx].pass_indices[k_adj] == consumer_idx) {
                        already_linked = ZEST_TRUE;
                        break;
                    }
                }
                if (!already_linked) {
                    zest_vec_linear_push(allocator, adjacency_list[producer_idx].pass_indices, consumer_idx);
                    dependency_count[consumer_idx]++; // consumer_idx gains an incoming edge
                }
            }
        }
    }
    
	//[Create_execution_waves]
	zest_execution_wave_t *initial_waves = 0;
    zest_execution_wave_t first_wave = ZEST__ZERO_INIT(zest_execution_wave_t);
    zest_uint pass_count = 0;

    int *dependency_queue = 0;
    zest_vec_foreach(i, dependency_count) {
        if (dependency_count[i] == 0) {
            zest_pass_group_t *pass = &frame_graph->final_passes.data[i];
            first_wave.queue_bits |= pass->queue_info.queue_type;
            zest_vec_linear_push(allocator, dependency_queue, i);
            zest_vec_linear_push(allocator, first_wave.pass_indices, i);
            pass_count++;
        }
    }
    if (pass_count > 0) {
		if (zloc__count_bits(first_wave.queue_bits) == 1) {
			zest_vec_foreach(i, first_wave.pass_indices) {
				zest_pass_group_t *pass = &frame_graph->final_passes.data[first_wave.pass_indices[i]];
				if (pass->compiled_queue_info.queue_type != zest_queue_graphics) {
					pass->compiled_queue_info.queue = &context->device->graphics_queue;
					pass->compiled_queue_info.queue_family_index = context->device->graphics_queue_family_index;
					pass->compiled_queue_info.queue_type = zest_queue_graphics;
				}
			}
			first_wave.queue_bits = zest_queue_graphics;
		}
        zest_vec_linear_push(allocator, initial_waves, first_wave);
    }

    zest_uint current_wave_index = 0;
    while (current_wave_index < zest_vec_size(initial_waves)) {
        zest_execution_wave_t next_wave = ZEST__ZERO_INIT(zest_execution_wave_t);
        zest_execution_wave_t *current_wave = &initial_waves[current_wave_index];
        next_wave.level = current_wave->level + 1;
        zest_vec_foreach(i, current_wave->pass_indices) {
            int pass_index_of_current_wave = current_wave->pass_indices[i];
            zest_vec_foreach(j, adjacency_list[pass_index_of_current_wave].pass_indices) {
                int dependent_pass_index = adjacency_list[pass_index_of_current_wave].pass_indices[j];
                dependency_count[dependent_pass_index]--;
                if (dependency_count[dependent_pass_index] == 0) {
					zest_pass_group_t *pass = &frame_graph->final_passes.data[dependent_pass_index];
                    next_wave.queue_bits |= pass->queue_info.queue_type;
                    zest_vec_linear_push(allocator, next_wave.pass_indices, dependent_pass_index);
                    pass_count++;
                }
            }
        }
        if (zest_vec_size(next_wave.pass_indices) > 0) {
			//If this wave is NOT an async wave (ie., only a single queue is used) then it should only
			//use the graphics queue.
			if (zloc__count_bits(next_wave.queue_bits) == 1) {
				zest_vec_foreach(i, next_wave.pass_indices) {
					zest_pass_group_t *pass = &frame_graph->final_passes.data[next_wave.pass_indices[i]];
					if (pass->compiled_queue_info.queue_type != zest_queue_graphics) {
						pass->compiled_queue_info.queue = &context->device->graphics_queue;
						pass->compiled_queue_info.queue_family_index = context->device->graphics_queue_family_index;
						pass->compiled_queue_info.queue_type = zest_queue_graphics;
					}
				}
				next_wave.queue_bits = zest_queue_graphics;
			}
			zest_vec_linear_push(allocator, initial_waves, next_wave);
        }
        current_wave_index++;
    }

    if (zest_vec_size(initial_waves) == 0) {
        //Nothing to send to the GPU!
        return frame_graph;
    }

	for (zest_uint i = 0; i < zest_vec_size(initial_waves); ++i) {
		zest_execution_wave_t *current_wave = &initial_waves[i];
		if (current_wave->queue_bits == zest_queue_graphics) {
			// This is a graphics wave. Start a new merged wave.
			zest_execution_wave_t merged_wave = *current_wave;
			// Look ahead to see how many subsequent waves are also graphics waves.
			zest_uint waves_to_merge = 0;
			for (zest_uint j = i + 1; j < zest_vec_size(initial_waves); ++j) {
				if (initial_waves[j].queue_bits == zest_queue_graphics) {
					// Merge the passes from the next graphics wave into our new merged_wave.
					zest_vec_foreach(k, initial_waves[j].pass_indices) {
						zest_vec_linear_push(allocator, merged_wave.pass_indices, initial_waves[j].pass_indices[k]);
					}
					waves_to_merge++;
				} else {
					// Stop when we hit a non-graphics (async) wave.
					break;
				}
			}
			// Add the big merged wave to our final list.
			zest_vec_linear_push(allocator, frame_graph->execution_waves, merged_wave);
			// Skip the outer loop ahead past the waves we just merged.
			i += waves_to_merge;
		} else {
			// This is a true async wave (mixed queues), so add it to the list as-is.
			zest_vec_linear_push(allocator, frame_graph->execution_waves, *current_wave);
		}
	}

    //Bug, pass count which is a count of all pass groups added to waves should equal the number of final passes.
    ZEST_ASSERT(pass_count == zest_map_size(frame_graph->final_passes));

    //Create_command_batches
    //We take the waves that we created that identified passes that can run in parallel on separate queues and 
    //organise them into wave submission batches:
    zest_wave_submission_t current_submission = ZEST__ZERO_INIT(zest_wave_submission_t);

    zest_bool interframe_has_waited[ZEST_QUEUE_COUNT] = { 0 };
    zest_bool interframe_has_signalled[ZEST_QUEUE_COUNT] = { 0 };

    zest_vec_foreach(wave_index, frame_graph->execution_waves) {
        zest_execution_wave_t *wave = &frame_graph->execution_waves[wave_index];
        //If the wave has more than one queue then the passes can run in parallel in separate submission batches
        zest_uint queue_type_count = zloc__count_bits(wave->queue_bits);
        //This should be impossible to hit, if no queue bit
        ZEST_ASSERT(queue_type_count);  //Must be using at lease 1 queue
        if (queue_type_count > 1) {
            if (current_submission.batches[0].magic || current_submission.batches[1].magic || current_submission.batches[2].magic) {
				zest_vec_linear_push(allocator, frame_graph->submissions, current_submission);
				current_submission = ZEST__ZERO_INIT(zest_wave_submission_t);
            }
            //Create parallel batches
            zest_vec_foreach(pass_index, wave->pass_indices) {
                zest_uint current_pass_index = wave->pass_indices[pass_index];
                zest_pass_group_t *pass = &frame_graph->final_passes.data[current_pass_index];
                zest_uint qi = zloc__scan_reverse(pass->compiled_queue_info.queue_type);
				if (!current_submission.batches[qi].magic) {
					current_submission.batches[qi].magic = zest_INIT_MAGIC(zest_struct_type_wave_submission);
					current_submission.batches[qi].backend = (zest_submission_batch_backend)context->device->platform->new_submission_batch_backend(context);
					current_submission.batches[qi].queue = pass->compiled_queue_info.queue;
					current_submission.batches[qi].queue_family_index = pass->compiled_queue_info.queue_family_index;
					current_submission.batches[qi].queue_type = pass->compiled_queue_info.queue_type;
					current_submission.batches[qi].need_timeline_wait = interframe_has_waited[qi] ? ZEST_FALSE : ZEST_TRUE;
				}
				current_submission.batches[qi].timeline_wait_stage |= pass->compiled_queue_info.timeline_wait_stage;
				if (ZEST__FLAGGED(pass->flags, zest_pass_flag_outputs_to_swapchain)) {
					current_submission.batches[qi].outputs_to_swapchain = ZEST_TRUE;
				}
				interframe_has_waited[qi] = ZEST_TRUE;
				current_submission.queue_bits |= pass->compiled_queue_info.queue_type;
                zest_vec_linear_push(allocator, current_submission.batches[qi].pass_indices, current_pass_index);
            }
            zest_vec_linear_push(allocator, frame_graph->submissions, current_submission);
            current_submission = ZEST__ZERO_INIT(zest_wave_submission_t);
        } else {
            //Waves that have no parallel submissions 
			zest_uint qi = zloc__scan_reverse(wave->queue_bits);
            if (!current_submission.batches[qi].magic && (current_submission.batches[0].magic || current_submission.batches[1].magic || current_submission.batches[2].magic)) {
                //The queue is different from the last wave, finalise this submission
				zest_vec_linear_push(allocator, frame_graph->submissions, current_submission);
				current_submission = ZEST__ZERO_INIT(zest_wave_submission_t);
            }
            zest_vec_foreach(pass_index, wave->pass_indices) {
                zest_uint current_pass_index = wave->pass_indices[pass_index];
                zest_pass_group_t *pass = &frame_graph->final_passes.data[current_pass_index];
				if (!current_submission.batches[qi].magic) {
					current_submission.batches[qi].magic = zest_INIT_MAGIC(zest_struct_type_wave_submission);
					current_submission.batches[qi].backend = (zest_submission_batch_backend)context->device->platform->new_submission_batch_backend(context);
					current_submission.batches[qi].queue = pass->compiled_queue_info.queue;
					current_submission.batches[qi].queue_family_index = pass->compiled_queue_info.queue_family_index;
					current_submission.batches[qi].queue_type = pass->compiled_queue_info.queue_type;
					current_submission.batches[qi].need_timeline_wait = interframe_has_waited[qi] ? ZEST_FALSE : ZEST_TRUE;
				}
				current_submission.batches[qi].timeline_wait_stage |= pass->compiled_queue_info.timeline_wait_stage;
				if (ZEST__FLAGGED(pass->flags, zest_pass_flag_outputs_to_swapchain)) {
					current_submission.batches[qi].outputs_to_swapchain = ZEST_TRUE;
				}
				interframe_has_waited[qi] = ZEST_TRUE;
				current_submission.queue_bits = pass->compiled_queue_info.queue_type;
                zest_vec_linear_push(allocator, current_submission.batches[qi].pass_indices, current_pass_index);
            }
        }
    }

    //Add the last batch that was being processed if it was a sequential one.
    if (current_submission.batches[0].magic || current_submission.batches[1].magic || current_submission.batches[2].magic) {
        zest_vec_linear_push(allocator, frame_graph->submissions, current_submission);
    }

    //[Resource_journeys]
    //Now that the passes have been grouped into wave submissions we can calculate the resource journey and set the
    //first and last usage index for each resource.
    zest_uint execution_order_index = 0;
    zest_vec_foreach(submission_index, frame_graph->submissions) {
        zest_wave_submission_t *current_wave = &frame_graph->submissions[submission_index];
        for (zest_uint queue_index = 0; queue_index != ZEST_QUEUE_COUNT; ++queue_index) {
            if (!current_wave->batches[queue_index].magic) continue;
            zest_submission_batch_t *batch = &current_wave->batches[queue_index];
            zest_vec_foreach(execution_index, batch->pass_indices) {
                zest_uint current_pass_index = batch->pass_indices[execution_index];
                zest_pass_group_t *current_pass = &frame_graph->final_passes.data[current_pass_index];
                current_pass->submission_id = ZEST__MAKE_SUBMISSION_ID(submission_index, execution_index, queue_index);

                zest_vec_linear_push(allocator, frame_graph->pass_execution_order, current_pass);

                zest_batch_key batch_key = ZEST__ZERO_INIT(zest_batch_key);
                batch_key.current_family_index = current_pass->compiled_queue_info.queue_family_index;

                //Calculate_lifetime_of_resources and also create a state for each resource and plot
                //it's journey through the frame graph so that the appropriate barriers and semaphores
                //can be set up
                zest_map_foreach(input_idx, current_pass->inputs) {
                    zest_resource_node resource_node = current_pass->inputs.data[input_idx].resource_node;
                    if (resource_node->aliased_resource) resource_node = resource_node->aliased_resource;
                    if (resource_node) {
                        resource_node->first_usage_pass_idx = ZEST__MIN(resource_node->first_usage_pass_idx, execution_order_index);
                        resource_node->last_usage_pass_idx = ZEST__MAX(resource_node->last_usage_pass_idx, execution_order_index);
                    }
                    if (resource_node->producer_pass_idx != -1) {
                        zest_pass_group_t *producer_pass = &frame_graph->final_passes.data[resource_node->producer_pass_idx];
                        zest_uint producer_queue_index = ZEST__QUEUE_INDEX(producer_pass->submission_id);
                        if (queue_index != producer_queue_index) {
                            batch->queue_wait_stages |= current_pass->inputs.data[input_idx].stage_mask;
                        }
                    }
                    zest_resource_state_t state = ZEST__ZERO_INIT(zest_resource_state_t);
                    state.pass_index = current_pass_index;
                    state.queue_family_index = current_pass->compiled_queue_info.queue_family_index;
                    state.usage = current_pass->inputs.data[input_idx];
                    state.submission_id = current_pass->submission_id;
                    zest_vec_linear_push(allocator, resource_node->journey, state);
                }

                // Check OUTPUTS of the current pass
                bool requires_new_batch = false;
                zest_map_foreach(output_idx, current_pass->outputs) {
                    zest_resource_node resource_node = current_pass->outputs.data[output_idx].resource_node;
                    if (resource_node->aliased_resource) resource_node = resource_node->aliased_resource;
                    if (resource_node) {
                        resource_node->first_usage_pass_idx = ZEST__MIN(resource_node->first_usage_pass_idx, execution_order_index);
                        resource_node->last_usage_pass_idx = ZEST__MAX(resource_node->last_usage_pass_idx, execution_order_index);
                    }
                    zest_resource_state_t state = ZEST__ZERO_INIT(zest_resource_state_t);
                    state.pass_index = current_pass_index;
                    state.queue_family_index = current_pass->compiled_queue_info.queue_family_index;
                    state.usage = current_pass->outputs.data[output_idx];
                    state.submission_id = current_pass->submission_id;
                    zest_vec_linear_push(allocator, resource_node->journey, state);
                    zest_vec_foreach(adjacent_index, adjacency_list[current_pass_index].pass_indices) {
                        zest_uint consumer_pass_index = adjacency_list[current_pass_index].pass_indices[adjacent_index];
                        batch_key.next_pass_indexes |= (1ull << consumer_pass_index);
                        batch_key.next_family_indexes |= (1ull << frame_graph->final_passes.data[consumer_pass_index].compiled_queue_info.queue_family_index);
                    }
                }
                execution_order_index++;
            }
        }
    }

	//[Create_semaphores]
    //Potentially connect the first submission that uses the swap chain image
    if (zest_vec_size(frame_graph->submissions) > 0) {
        // --- Handle imageAvailableSemaphore ---
        if (ZEST__FLAGGED(frame_graph->flags, zest_frame_graph_expecting_swap_chain_usage)) {

            zest_submission_batch_t *first_batch_to_wait = NULL;
            zest_pipeline_stage_flags wait_stage_for_acquire_semaphore = zest_pipeline_stage_top_of_pipe_bit; // Safe default

            zest_resource_node swapchain_node = frame_graph->swapchain_resource; 
            if (swapchain_node->first_usage_pass_idx != ZEST_INVALID) {
                zest_pass_group_t *pass = frame_graph->pass_execution_order[swapchain_node->first_usage_pass_idx];
                zest_uint submission_index = ZEST__SUBMISSION_INDEX(pass->submission_id);
                zest_uint execution_index = ZEST__EXECUTION_INDEX(pass->submission_id);
                zest_uint queue_index = ZEST__QUEUE_INDEX(pass->submission_id);
                zest_wave_submission_t *wave_submission = &frame_graph->submissions[submission_index];
                zest_uint pass_index = wave_submission->batches[queue_index].pass_indices[execution_index];
				zest_bool uses_swapchain = ZEST_FALSE;
				zest_pipeline_stage_flags first_swapchain_usage_stage_in_this_batch = zest_pipeline_stage_top_of_pipe_bit;
                if (zest_map_valid_name(pass->inputs, swapchain_node->name)) {
					zest_resource_usage_t *usage = zest_map_at(pass->inputs, swapchain_node->name);
					uses_swapchain = ZEST_TRUE;
					first_swapchain_usage_stage_in_this_batch = usage->stage_mask;
                } else if (zest_map_valid_name(pass->outputs, swapchain_node->name)) {
					zest_resource_usage_t *usage = zest_map_at(pass->outputs, swapchain_node->name);
					uses_swapchain = ZEST_TRUE;
					first_swapchain_usage_stage_in_this_batch = usage->stage_mask;
                }
                if (uses_swapchain) {
                    first_batch_to_wait = &wave_submission->batches[queue_index];
                    wait_stage_for_acquire_semaphore = first_swapchain_usage_stage_in_this_batch;
                    // Ensure this stage is compatible with the batch's queue
                    if (!zest__is_stage_compatible_with_qfi(wait_stage_for_acquire_semaphore, context->device->queues[queue_index]->type)) {
                        ZEST_PRINT("Swapchain usage stage %i is not compatible with queue family %u for wave submission %i",
                            wait_stage_for_acquire_semaphore,
                            first_batch_to_wait->queue_family_index, submission_index);
                        // Fallback or error. Forcing TOP_OF_PIPE might be safer if this happens,
                        // though it indicates a graph definition error.
                        wait_stage_for_acquire_semaphore = zest_pipeline_stage_top_of_pipe_bit;
                    }
                }
            }

            // 2. Assign the wait:
            if (first_batch_to_wait) {
                // The first batch that actually uses the swapchain will wait for it.
				zest_semaphore_reference_t semaphore_reference = { zest_dynamic_resource_image_available_semaphore, 0 };
                zest_vec_linear_push(allocator, first_batch_to_wait->wait_semaphores, semaphore_reference);
                zest_vec_linear_push(allocator, first_batch_to_wait->wait_dst_stage_masks, wait_stage_for_acquire_semaphore);
            } else {
                // Image was acquired, but no pass in the graph uses it.
                // The *very first submission batch of the graph* must wait on the semaphore to consume it.
                // The wait stage must be compatible with this first batch's queue.
                zest_wave_submission_t *actual_first_wave = &frame_graph->submissions[0];
                zest_pipeline_stage_flags compatible_dummy_wait_stage = zest_pipeline_stage_top_of_pipe_bit;
                zest_uint queue_family_index = ZEST_INVALID;
                zest_submission_batch_t *first_batch = NULL;

                // Make the dummy wait stage more specific if possible, but ensure compatibility
                for (int queue_index = 0; queue_index != ZEST_QUEUE_COUNT; ++queue_index) {
                    if (actual_first_wave->batches[queue_index].magic) {
                        first_batch = &actual_first_wave->batches[queue_index];
                        if (context->device->queues[queue_index]->type & zest_queue_transfer) {
                            compatible_dummy_wait_stage = zest_pipeline_stage_transfer_bit;
                            queue_family_index = actual_first_wave->batches[queue_index].queue_family_index;
                        } else if (context->device->queues[queue_index]->type & zest_queue_compute) {
                            compatible_dummy_wait_stage = zest_pipeline_stage_compute_shader_bit;
                            queue_family_index = actual_first_wave->batches[queue_index].queue_family_index;
                        }
                        break;
                    }
                }
                // Graphics queue can also do TOP_OF_PIPE or even COLOR_ATTACHMENT_OUTPUT_BIT if it's just for semaphore flow.

                if (first_batch) {
                    zest_semaphore_reference_t semaphore_reference = { zest_dynamic_resource_image_available_semaphore, 0 };
                    zest_vec_linear_push(allocator, first_batch->wait_semaphores, semaphore_reference);
                    zest_vec_linear_push(allocator, first_batch->wait_dst_stage_masks, compatible_dummy_wait_stage);
                    ZEST_PRINT("RenderGraph: Swapchain image acquired but not used by any pass. First batch (on QFI %u) will wait on imageAvailableSemaphore at stage %i.",
                        queue_family_index,
                        compatible_dummy_wait_stage);
                }
            }
        }

        // --- Handle renderFinishedSemaphore for the last batch ---
        zest_wave_submission_t *last_wave = &zest_vec_back(frame_graph->submissions);
		if (last_wave->batches[ZEST_GRAPHICS_QUEUE_INDEX].magic && last_wave->batches[ZEST_GRAPHICS_QUEUE_INDEX].outputs_to_swapchain == ZEST_TRUE) {  //Only if it's a graphics queue and it actually outputs to the swapchain
            // This assumes the last batch's *primary* signal is renderFinished.
            if (!last_wave->batches[ZEST_GRAPHICS_QUEUE_INDEX].signal_semaphores) {
				zest_semaphore_reference_t semaphore_reference = { zest_dynamic_resource_render_finished_semaphore, 0 };
                zest_vec_linear_push(allocator, last_wave->batches[ZEST_GRAPHICS_QUEUE_INDEX].signal_semaphores, semaphore_reference);
            } else {
                // This case needs `p_signal_semaphores` to be a list in your batch struct.
                // You would then add context->renderer->frame_sync[context->current_fif].render_finished_semaphore to that list.
                ZEST_PRINT("Last batch already has an internal signal_semaphore. Logic to add external renderFinishedSemaphore needs p_signal_semaphores to be a list.");
                // For now, you might just overwrite if single signal is assumed for external:
                // last_batch->internal_signal_semaphore = context->renderer->frame_sync[context->current_fif].render_finished_semaphore;
            }
        }
    }

    //Plan_transient_buffers
    zest_bucket_array_foreach(resource_index, frame_graph->resources) {
        zest_resource_node resource = zest_bucket_array_get(&frame_graph->resources, zest_resource_node_t, resource_index);

        // Check if this resource is transient AND actually used in the compiled graph
        if (ZEST__FLAGGED(resource->flags, zest_resource_node_flag_transient)) {
            if (resource->reference_count > 0 || ZEST__FLAGGED(resource->flags, zest_resource_node_flag_essential_output) &&
                resource->first_usage_pass_idx <= resource->last_usage_pass_idx && // Ensures it's used
                resource->first_usage_pass_idx != ZEST_INVALID) {

                zest_pass_group_t *first_pass = frame_graph->pass_execution_order[resource->first_usage_pass_idx];
                zest_pass_group_t *last_pass = frame_graph->pass_execution_order[resource->last_usage_pass_idx];

                zest_vec_linear_push(allocator, first_pass->transient_resources_to_create, resource);
                zest_vec_linear_push(allocator, last_pass->transient_resources_to_free, resource);
            } else {
                frame_graph->culled_resources_count++;
                ZEST__REPORT(context, zest_report_resource_culled, "Transient resource [%s] was culled because it's was not consumed by any other passes. If you intended to use this output outside of the frame graph once it's executed then you can call zest_FlagResourceAsEssential.", resource->name);
            }
        } 
        if (resource->buffer_provider || resource->image_provider) {
            zest_vec_linear_push(allocator, frame_graph->resources_to_update, resource);
        }
    }

    //Plan_resource_barriers
    zest_bucket_array_foreach(resource_index, frame_graph->resources) {
        zest_resource_node resource = zest_bucket_array_get(&frame_graph->resources, zest_resource_node_t, resource_index);
        if (resource->aliased_resource) continue;
        zest_resource_state_t *prev_state = NULL;
        int starting_state_index = 0;
        zest_vec_foreach(state_index, resource->journey) {
            if (resource->reference_count == 0 && ZEST__NOT_FLAGGED(resource->flags, zest_resource_node_flag_essential_output)) {
                continue;
            }
            zest_resource_state_t *current_state = &resource->journey[state_index];
            zest_pass_group_t *pass = &frame_graph->final_passes.data[current_state->pass_index];
            zest_resource_usage_t *current_usage = &current_state->usage;
            zest_resource_state_t *next_state = NULL;
            zest_execution_details_t *exe_details = &pass->execution_details;
            zest_execution_barriers_t *barriers = &exe_details->barriers;
            if (state_index + 1 < (int)zest_vec_size(resource->journey)) {
                next_state = &resource->journey[state_index + 1];
            }
            zest_pipeline_stage_flags required_stage = current_usage->stage_mask;
            if (resource->type & zest_resource_type_is_image) {
                zest__add_image_barriers(frame_graph, allocator, resource, barriers, current_state, prev_state, next_state);
                prev_state = current_state;
                if (current_state->usage.access_mask & zest_access_render_pass_bits) {
                    exe_details->requires_dynamic_render_pass = true;
                }
            } else if(resource->type & zest_resource_type_buffer) {
                if (!prev_state) {
                    //This is the first state of the resource
                    //If there's no previous state then we need to see if a barrier is needed to transition from the resource
                    //start state. We put this in the acquire barrier as it needs to be put in place before the pass is executed.
                    zest_uint src_queue_family_index = resource->current_queue_family_index;
                    zest_uint dst_queue_family_index = ZEST_QUEUE_FAMILY_IGNORED;
                    if (src_queue_family_index == ZEST_QUEUE_FAMILY_IGNORED) {
                        if ((resource->access_mask & zest_access_write_bits_general) &&
                            (current_usage->access_mask & zest_access_read_bits_general)) {
                            context->device->platform->add_frame_graph_buffer_barrier(resource, barriers, true,
                                resource->access_mask, current_usage->access_mask,
                                src_queue_family_index, dst_queue_family_index,
                                resource->last_stage_mask, current_state->usage.stage_mask);
							#ifdef ZEST_TEST_MODE
                            zest__add_buffer_barrier(resource, barriers, true,
                                resource->access_mask, current_usage->access_mask,
                                src_queue_family_index, dst_queue_family_index,
                                resource->last_stage_mask, current_state->usage.stage_mask);
							#endif
                        }
                    } else {
                        //This resource already belongs to a queue which means that it's an imported resource
                        //If the frame graph is on the graphics queue only then there's no need to acquire from a prior release.

                        //It shouldn't be possible for transient resources to be considered for a barrier at this point. 
                        ZEST_ASSERT(ZEST__FLAGGED(resource->flags, zest_resource_node_flag_imported));
                        dst_queue_family_index = current_state->queue_family_index;
                        if (src_queue_family_index != ZEST_QUEUE_FAMILY_IGNORED) {
                            context->device->platform->add_frame_graph_buffer_barrier(resource, barriers, true,
                                resource->access_mask, current_usage->access_mask,
                                src_queue_family_index, dst_queue_family_index,
                                resource->last_stage_mask, current_state->usage.stage_mask);
							#ifdef ZEST_TEST_MODE
                            zest__add_buffer_barrier(resource, barriers, true,
                                resource->access_mask, current_usage->access_mask,
                                src_queue_family_index, dst_queue_family_index,
                                resource->last_stage_mask, current_state->usage.stage_mask);
							#endif
                        }
                    }
                    bool needs_releasing = false;
                } else {
                    //There is a previous state, do we need to acquire the resource from a different queue?
                    zest_uint src_queue_family_index = ZEST_QUEUE_FAMILY_IGNORED;
                    zest_uint dst_queue_family_index = ZEST_QUEUE_FAMILY_IGNORED;
                    bool needs_acquiring = false;
                    if (prev_state && (prev_state->queue_family_index != current_state->queue_family_index || prev_state->was_released)) {
                        //The next state is in a different queue so the resource needs to be released to that queue
                        src_queue_family_index = prev_state->queue_family_index;
                        dst_queue_family_index = current_state->queue_family_index;
                        needs_acquiring = true;
                    }
                    if (needs_acquiring) {
                        zest_resource_usage_t *prev_usage = &prev_state->usage;
                        //Acquire the resource. No transitioning is done here, acquire only if needed
                        context->device->platform->add_frame_graph_buffer_barrier(resource, barriers, true,
                            prev_usage->access_mask, current_usage->access_mask,
                            src_queue_family_index, dst_queue_family_index,
                            prev_state->usage.stage_mask, current_state->usage.stage_mask);
						#ifdef ZEST_TEST_MODE
                        zest__add_buffer_barrier(resource, barriers, true,
                            prev_usage->access_mask, current_usage->access_mask,
                            src_queue_family_index, dst_queue_family_index,
                            prev_state->usage.stage_mask, current_state->usage.stage_mask);
						#endif
                        zest_submission_batch_t *batch = zest__get_submission_batch(current_state->submission_id);
                        batch->queue_wait_stages |= prev_state->usage.stage_mask;
                    }
                }
                if (next_state) {
                    //If the resource has a state after this one then we can check if a barrier is needed to transition and/or
                    //release to another queue family
                    zest_uint src_queue_family_index = current_state->queue_family_index;
                    zest_uint dst_queue_family_index = next_state->queue_family_index;
                    zest_resource_usage_t *next_usage = &next_state->usage;
                    bool needs_releasing = false;
                    if (next_state && next_state->queue_family_index != current_state->queue_family_index) {
                        //The next state is in a different queue so the resource needs to be released to that queue
                        src_queue_family_index = current_state->queue_family_index;
                        dst_queue_family_index = next_state->queue_family_index;
                        needs_releasing = true;
                    }
                    if (needs_releasing || 
                        (current_usage->access_mask & zest_access_write_bits_general) &&
                        (next_usage->access_mask & zest_access_read_bits_general)) {
                        zest_pipeline_stage_flags dst_stage = zest_pipeline_stage_bottom_of_pipe_bit;
                        if (needs_releasing) {
                            current_state->was_released = true;
                        } else {
                            dst_stage = next_state->usage.stage_mask;
                            current_state->was_released = false;
                        }
                        context->device->platform->add_frame_graph_buffer_barrier(resource, barriers, false,
                            current_usage->access_mask, next_state->usage.access_mask,
                            src_queue_family_index, dst_queue_family_index,
                            current_state->usage.stage_mask, dst_stage);
						#ifdef ZEST_TEST_MODE
                        zest__add_buffer_barrier(resource, barriers, false,
                            current_usage->access_mask, next_state->usage.access_mask,
                            src_queue_family_index, dst_queue_family_index,
                            current_state->usage.stage_mask, dst_stage);
						#endif
                    }
                } else if (resource->flags & zest_resource_node_flag_release_after_use
                    && current_state->queue_family_index != context->device->transfer_queue_family_index) {
                    //Release the buffer so that it's ready to be acquired by any other queue in the next frame
                    //Release to the transfer queue by default (if it's not already on the transfer queue).
					context->device->platform->add_frame_graph_buffer_barrier(resource, barriers, false,
						current_usage->access_mask, zest_access_none,
                        current_state->queue_family_index, context->device->transfer_queue_family_index,
                        current_state->usage.stage_mask, zest_pipeline_stage_bottom_of_pipe_bit);
					#ifdef ZEST_TEST_MODE
					zest__add_buffer_barrier(resource, barriers, false,
						current_usage->access_mask, zest_access_none,
                        current_state->queue_family_index, context->device->transfer_queue_family_index,
                        current_state->usage.stage_mask, zest_pipeline_stage_bottom_of_pipe_bit);
					#endif
                }
                prev_state = current_state;
            }
 
        }
    }

    //Process_compiled_execution_order
	//Prepare_render_pass
    zest_vec_foreach(submission_index, frame_graph->submissions) {
        zest_wave_submission_t *submission = &frame_graph->submissions[submission_index];
        for (int queue_index = 0; queue_index != ZEST_QUEUE_COUNT; ++queue_index) {
            zest_vec_foreach(batch_index, submission->batches[queue_index].pass_indices) {
                int current_pass_index = submission->batches[queue_index].pass_indices[batch_index];
                zest_pass_group_t *pass = &frame_graph->final_passes.data[current_pass_index];
                zest_execution_details_t *exe_details = &pass->execution_details;

                context->device->platform->validate_barrier_pipeline_stages(&exe_details->barriers);

				zest__prepare_render_pass(pass, exe_details, current_pass_index);

            }   //Passes within batch loop
        }
        
    }   //Batch loop

    if (ZEST__FLAGGED(frame_graph->flags, zest_frame_graph_expecting_swap_chain_usage)) {
        if (ZEST__NOT_FLAGGED(frame_graph->flags, zest_frame_graph_present_after_execute)) {
            ZEST__REPORT(context, zest_report_expecting_swapchain_usage, "Swapchain usage was expected but the frame graph present flag was not set in frame graph [%s], indicating that a render pass could not be created. Check other reports.", frame_graph->name);
            ZEST__FLAG(frame_graph->flags, zest_frame_graph_present_after_execute);
        }
        //Error: the frame graph is trying to render to the screen but no swap chain image was used!
        //Make sure that you call zest_ConnectSwapChainOutput in your frame graph setup.
    }
	ZEST__FLAG(frame_graph->flags, zest_frame_graph_is_compiled);  

    return frame_graph;
}

void zest__prepare_render_pass(zest_pass_group_t *pass, zest_execution_details_t *exe_details, zest_uint current_pass_index) {
	if (exe_details->requires_dynamic_render_pass) {
		zest_context context = zest__frame_graph_builder->context;
		zloc_linear_allocator_t *allocator = context->frame_graph_allocator[context->current_fif];
		zest_uint color_attachment_index = 0;
		//Determine attachments for color and depth (resolve can come later), first for outputs
		exe_details->depth_attachment.image_view = 0;
		exe_details->rendering_info.sample_count = zest_sample_count_1_bit;
		zest_map_foreach(o, pass->outputs) {
			zest_resource_usage_t *output_usage = &pass->outputs.data[o];
			zest_resource_node resource = pass->outputs.data[o].resource_node;
			if (resource->type & zest_resource_type_is_image) {
				if (resource->type != zest_resource_type_depth && ZEST__FLAGGED(pass->flags, zest_pass_flag_output_resolve) && resource->image.info.sample_count == 1) {
					output_usage->purpose = zest_purpose_color_attachment_resolve;
				}
				if (output_usage->purpose == zest_purpose_color_attachment_write) {
					zest_rendering_attachment_info_t color = ZEST__ZERO_INIT(zest_rendering_attachment_info_t);
					//ZEST_ASSERT(resource->view);
					color.image_view = &resource->view;
					color.layout = output_usage->image_layout;
					color.load_op = output_usage->load_op;
					color.store_op = output_usage->store_op;
					color.clear_value = output_usage->clear_value;
					exe_details->rendering_info.color_attachment_formats[color_attachment_index] = resource->image.info.format;
					zest_vec_linear_push(allocator, exe_details->color_attachments, color);
					if (color_attachment_index == 0) {
						exe_details->render_area.offset.x = 0;
						exe_details->render_area.offset.y = 0;
						exe_details->render_area.extent.width = resource->image.info.extent.width;
						exe_details->render_area.extent.height = resource->image.info.extent.height;
					}
					color_attachment_index++;
					exe_details->rendering_info.color_attachment_count++;
					//zest_image_layout final_layout = zest__determine_final_layout(zest__determine_final_layout(current_pass_index, resource, output_usage));
					if (resource->type == zest_resource_type_swap_chain_image) {
						context->device->platform->add_frame_graph_image_barrier(
							resource, &exe_details->barriers, ZEST_FALSE,
							output_usage->access_mask, zest_access_none,
							output_usage->image_layout, zest_image_layout_present,
							resource->current_queue_family_index, resource->current_queue_family_index,
							output_usage->stage_mask, zest_pipeline_stage_bottom_of_pipe_bit
						);
						ZEST__FLAG(zest__frame_graph_builder->frame_graph->flags, zest_frame_graph_present_after_execute);
					}
				} else if (output_usage->purpose == zest_purpose_color_attachment_resolve) {
					exe_details->color_attachments[color_attachment_index].resolve_layout = output_usage->image_layout;
					exe_details->color_attachments[color_attachment_index].resolve_image_view = &resource->view;
					exe_details->rendering_info.sample_count = context->device->platform->get_msaa_sample_count(context);
				} else if (output_usage->purpose == zest_purpose_depth_stencil_attachment_write) {
					zest_rendering_attachment_info_t *depth = &exe_details->depth_attachment;
					*depth = ZEST__ZERO_INIT(zest_rendering_attachment_info_t);
					depth->image_view = &resource->view;
					depth->layout = output_usage->image_layout;
					depth->load_op = output_usage->load_op;
					depth->store_op = output_usage->store_op;
					depth->clear_value = output_usage->clear_value;
					exe_details->rendering_info.depth_attachment_format = resource->image.info.format;
					ZEST__FLAG(resource->flags, zest_resource_node_flag_used_in_output);
				}
			}
		}
	}
}

zest_frame_graph zest_EndFrameGraph() {
    zest_frame_graph frame_graph = zest__compile_frame_graph();
	zest_context context = zest__frame_graph_builder->context;

    ZEST__MAYBE_REPORT(context, zest__frame_graph_builder->current_pass, zest_report_missing_end_pass, "Warning in frame graph [%s]: The current pass in the frame graph context is not null. This means that a call to zest_EndPass is missing in the frame graph setup.", frame_graph->name);
    zest__frame_graph_builder->current_pass = 0;

    if (frame_graph->error_status != zest_fgs_critical_error) {
        zest__cache_frame_graph(frame_graph);
    } else {
        ZEST__UNFLAG(context->flags, zest_context_flag_work_was_submitted);
    }

    zest__frame_graph_builder = NULL;
	ZEST__UNFLAG(context->flags, zest_context_flag_building_frame_graph);  

    return frame_graph;
}

zest_frame_graph zest_EndFrameGraphAndWait() {
    zest_frame_graph frame_graph = zest__compile_frame_graph();

	zest_context context = zest__frame_graph_builder->context;

    if (frame_graph->error_status != zest_fgs_critical_error) {
        zest__execute_frame_graph(context, frame_graph, ZEST_TRUE);
    } else {
        ZEST__UNFLAG(context->flags, zest_context_flag_work_was_submitted);
    }

    zest__frame_graph_builder = NULL;
	ZEST__UNFLAG(context->flags, zest_context_flag_building_frame_graph);  

    return frame_graph;
}

zest_image_aspect_flags zest__determine_aspect_flag(zest_format format) {
    switch (format) {
        // Depth-Only Formats
    case zest_format_d16_unorm:
    case zest_format_x8_d24_unorm_pack32: // D24_UNORM component, X8 is undefined
    case zest_format_d32_sfloat:
        return zest_image_aspect_depth_bit;
        // Stencil-Only Formats
    case zest_format_s8_uint:
        return zest_image_aspect_stencil_bit;
        // Combined Depth/Stencil Formats
    case zest_format_d16_unorm_s8_uint:
    case zest_format_d24_unorm_s8_uint:
    case zest_format_d32_sfloat_s8_uint:
        return zest_image_aspect_depth_bit | zest_image_aspect_stencil_bit;
    default:
        return zest_image_aspect_color_bit;
    }
}

void zest__interpret_hints(zest_resource_node resource, zest_resource_usage_hint usage_hints) {
    if (usage_hints & zest_resource_usage_hint_copy_dst) {
        resource->image.info.flags |= zest_image_flag_transfer_dst;
    }
    if (usage_hints & zest_resource_usage_hint_copy_src) {
        resource->image.info.flags |= zest_image_flag_transfer_src;
    }
    if (usage_hints & zest_resource_usage_hint_cpu_transfer) {
        resource->image.info.flags |= zest_image_flag_host_visible | zest_image_flag_host_coherent;
        resource->image.info.flags |= zest_image_flag_transfer_src | zest_image_flag_transfer_dst;
    }
}

zest_image_layout zest__determine_final_layout(zest_uint pass_index, zest_resource_node node, zest_resource_usage_t *current_usage) {
    zest_frame_graph frame_graph = zest__frame_graph_builder->frame_graph;
    ZEST_ASSERT_HANDLE(frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    zest_vec_foreach(state_index, node->journey) {
        zest_resource_state_t *state = &node->journey[state_index];
        if (state->pass_index == pass_index && (zest_uint)state_index < zest_vec_size(node->journey) - 1) {
            return node->journey[state_index + 1].usage.image_layout;
        }
    }
    if (node->type == zest_resource_type_swap_chain_image) {
        return zest_image_layout_present;
    } else if (ZEST__FLAGGED(node->flags, zest_resource_node_flag_imported)) {
        return node->image_layout; 
    } 
    if (current_usage->store_op == zest_store_op_dont_care) {
        return zest_image_layout_undefined;
    }
    return zest_image_layout_shader_read_only_optimal;
}

void zest__deferr_resource_destruction(zest_context context, void* handle) {
    zest_vec_push(context->device->allocator, context->device->deferred_resource_freeing_list.resources[context->current_fif], handle);
}

void zest__deferr_image_destruction(zest_context context, zest_image image) {
    zest_vec_push(context->device->allocator, context->device->deferred_resource_freeing_list.images[context->current_fif], *image);
}

void zest__deferr_view_destruction(zest_context context, zest_image_view view) {
    zest_vec_push(context->device->allocator, context->device->deferred_resource_freeing_list.views[context->current_fif], *view);
}

void zest__deferr_view_array_destruction(zest_context context, zest_image_view_array view_array) {
    zest_vec_push(context->device->allocator, context->device->deferred_resource_freeing_list.view_arrays[context->current_fif], *view_array);
}

zest_bool zest__execute_frame_graph(zest_context context, zest_frame_graph frame_graph, zest_bool is_intraframe) {
    ZEST_ASSERT_HANDLE(frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    zloc_linear_allocator_t *allocator = context->frame_graph_allocator[context->current_fif];
    zest_map_queue_value queues = ZEST__ZERO_INIT(zest_map_queue_value);

    zest_execution_backend backend = (zest_execution_backend)context->device->platform->new_execution_backend(allocator);

    context->device->platform->set_execution_fence(context, backend, is_intraframe);

	zest_vec_foreach(resource_index, frame_graph->resources_to_update) {
		zest_resource_node resource = frame_graph->resources_to_update[resource_index];
		if (resource->buffer_provider) {
			resource->storage_buffer = resource->buffer_provider(context, resource);
		} else if (resource->image_provider) {
            resource->view = resource->image_provider(context, resource);
		}
	}

    zest_vec_foreach(submission_index, frame_graph->submissions) {
        zest_wave_submission_t *wave_submission = &frame_graph->submissions[submission_index];

        for (zest_uint queue_index = 0; queue_index != ZEST_QUEUE_COUNT; ++queue_index) {
            zest_submission_batch_t *batch = &wave_submission->batches[queue_index];
            if (!batch->magic) continue;
            ZEST_ASSERT(zest_vec_size(batch->pass_indices));    //A batch was created without any pass indices. Bug in the Compile stage!

            zest_pipeline_stage_flags timeline_wait_stage;

            // 1. acquire an appropriate command buffer
            switch (batch->queue_type) {
            case zest_queue_graphics:
                ZEST_CLEANUP_ON_FALSE(context->device->platform->set_next_command_buffer(&frame_graph->command_list, batch->queue));
                timeline_wait_stage = zest_pipeline_stage_vertex_input_bit;
                break;
            case zest_queue_compute:
                ZEST_CLEANUP_ON_FALSE(context->device->platform->set_next_command_buffer(&frame_graph->command_list, batch->queue));
                timeline_wait_stage = zest_pipeline_stage_compute_shader_bit;
                break;
            case zest_queue_transfer:
                ZEST_CLEANUP_ON_FALSE(context->device->platform->set_next_command_buffer(&frame_graph->command_list, batch->queue));
                timeline_wait_stage = zest_pipeline_stage_transfer_bit;
                break;
            default:
                ZEST_ASSERT(0); //Unknown queue type for batch. Corrupt memory perhaps?!
            }

            ZEST_CLEANUP_ON_FALSE(context->device->platform->begin_command_buffer(&frame_graph->command_list));

            zest_vec_foreach(i, batch->pass_indices) {
                zest_uint pass_index = batch->pass_indices[i];
                zest_pass_group_t *grouped_pass = &frame_graph->final_passes.data[pass_index];
                zest_execution_details_t *exe_details = &grouped_pass->execution_details;

                frame_graph->command_list.submission_index = submission_index;
                frame_graph->command_list.timeline_wait_stage = timeline_wait_stage;
                frame_graph->command_list.queue_index = queue_index;

                //Create any transient resources where they're first used in this grouped_pass
                zest_vec_foreach(r, grouped_pass->transient_resources_to_create) {
                    zest_resource_node resource = grouped_pass->transient_resources_to_create[r];
                    if (!zest__create_transient_resource(context, resource)) {
                        frame_graph->error_status |= zest_fgs_transient_resource_failure;
                        return ZEST_FALSE;
                    }
                }

                //Batch execute acquire barriers for images and buffers
				context->device->platform->acquire_barrier(&frame_graph->command_list, exe_details);

                bool has_render_pass = exe_details->requires_dynamic_render_pass;

                //Begin the render pass if the pass has one
                if (has_render_pass) {
                    context->device->platform->begin_render_pass(&frame_graph->command_list, exe_details);
					frame_graph->command_list.rendering_info = exe_details->rendering_info;
					frame_graph->command_list.began_rendering = ZEST_TRUE;
                }

                //Execute the callbacks in the pass
                zest_vec_foreach(pass_index, grouped_pass->passes) {
                    zest_pass_node pass = grouped_pass->passes[pass_index];
                    if (pass->type == zest_pass_type_graphics && !frame_graph->command_list.began_rendering) {
                        ZEST__REPORT(context, zest_report_render_pass_skipped, "Pass execution was skipped for pass [%s] becuase rendering did not start. Check for validation errors.", pass->name);
                        continue;
                    }
                    frame_graph->command_list.pass_node = pass;
                    frame_graph->command_list.frame_graph = frame_graph;
                    pass->execution_callback.callback(&frame_graph->command_list, pass->execution_callback.user_data);
                }

                zest_map_foreach(pass_input_index, grouped_pass->inputs) {
                    zest_resource_node resource = grouped_pass->inputs.data[pass_input_index].resource_node;
                    if (resource->aliased_resource) {
                        resource->aliased_resource->current_state_index++;
                    } else {
                        resource->current_state_index++;
                    }
                }

                zest_map_foreach(pass_output_index, grouped_pass->outputs) {
                    zest_resource_node resource = grouped_pass->outputs.data[pass_output_index].resource_node;
                    if (resource->aliased_resource) {
                        resource->aliased_resource->current_state_index++;
                    } else {
                        resource->current_state_index++;
                    }
                }

                if (has_render_pass) {
                    context->device->platform->end_render_pass(&frame_graph->command_list);
					frame_graph->command_list.began_rendering = ZEST_FALSE;
                }

                //Batch execute release barriers for images and buffers

				context->device->platform->release_barrier(&frame_graph->command_list, exe_details);

                zest_vec_foreach(r, grouped_pass->transient_resources_to_free) {
                    zest_resource_node resource = grouped_pass->transient_resources_to_free[r];
                    zest__free_transient_resource(resource);
                }
                //End pass
            }
			context->device->platform->end_command_buffer(&frame_graph->command_list);

            context->device->platform->submit_frame_graph_batch(frame_graph, backend, batch, &queues);

        }   //Batch

        //For each batch in the last wave add the queue semaphores that were used so that the next batch submissions can wait on them
        context->device->platform->carry_over_semaphores(frame_graph, wave_submission, backend);
    }   //Wave 

    zest_map_foreach(i, queues) {
        zest_queue queue = queues.data[i];
        queue->fif = (queue->fif + 1) % ZEST_MAX_FIF;
    }

	zest_bucket_array_foreach(index, frame_graph->resources) {
		zest_resource_node resource = zest_bucket_array_get(&frame_graph->resources, zest_resource_node_t, index);
        //Reset the resource state indexes (the point in their graph journey). This is necessary for cached
        //frame graphs.
        if (resource->aliased_resource) {
            resource->aliased_resource->current_state_index = 0;
        } else {
            resource->current_state_index = 0;
        }
	}

	zest_vec_foreach(i, frame_graph->deferred_image_destruction) {
		zest_resource_node resource = frame_graph->deferred_image_destruction[i];
		zest__deferr_image_destruction(context, &resource->image);
		if (resource->view_array) zest__deferr_view_array_destruction(context, resource->view_array);
		resource->image.mip_indexes = ZEST__ZERO_INIT(zest_map_mip_indexes);
		resource->view_array = 0;
		resource->view = 0;
		resource->mip_level_bindless_indexes = 0;
	}
	frame_graph->deferred_image_destruction = 0;

    ZEST__FLAG(frame_graph->flags, zest_frame_graph_is_executed);

    if (is_intraframe) {
        //Todo: handle this better
        if (!context->device->platform->frame_graph_fence_wait(context, backend)) {
            return ZEST_FALSE;
        }
    }

    return ZEST_TRUE;

cleanup:
    return ZEST_FALSE;
}

zest_key zest_GetPassOutputKey(zest_pass_node pass) {
    ZEST_ASSERT_HANDLE(pass);   //Not a valid pass node handle
    return pass->output_key;
}

bool zest_RenderGraphWasExecuted(zest_frame_graph frame_graph) {
    return ZEST__FLAGGED(frame_graph->flags, zest_frame_graph_is_executed);
}

void zest_PrintCachedFrameGraph(zest_context context, zest_frame_graph_cache_key_t *cache_key) {
    zest_frame_graph frame_graph = zest_GetCachedFrameGraph(context, cache_key);
    if (frame_graph) {
        zest_PrintCompiledFrameGraph(frame_graph);
    }
}

zest_frame_graph_result zest_GetFrameGraphResult(zest_frame_graph frame_graph) {
    ZEST_ASSERT_HANDLE(frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    return frame_graph->error_status;
}

zest_uint zest_GetFrameGraphCulledResourceCount(zest_frame_graph frame_graph) {
    ZEST_ASSERT_HANDLE(frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    return frame_graph->culled_resources_count;
}

zest_uint zest_GetFrameGraphFinalPassCount(zest_frame_graph frame_graph) {
    ZEST_ASSERT_HANDLE(frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    return zest_map_size(frame_graph->final_passes);
}

zest_uint zest_GetFrameGraphPassTransientCreateCount(zest_frame_graph frame_graph, zest_key output_key) {
    ZEST_ASSERT_HANDLE(frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    if (zest_map_valid_key(frame_graph->final_passes, output_key)) {
		zest_pass_group_t *final_pass = zest_map_at_key(frame_graph->final_passes, output_key);
        return zest_vec_size(final_pass->transient_resources_to_create);
    }
    return 0;
}

zest_uint zest_GetFrameGraphPassTransientFreeCount(zest_frame_graph frame_graph, zest_key output_key) {
    ZEST_ASSERT_HANDLE(frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    if (zest_map_valid_key(frame_graph->final_passes, output_key)) {
		zest_pass_group_t *final_pass = zest_map_at_key(frame_graph->final_passes, output_key);
        return zest_vec_size(final_pass->transient_resources_to_free);
    }
    return 0;
}

zest_uint zest_GetFrameGraphCulledPassesCount(zest_frame_graph frame_graph) {
    ZEST_ASSERT_HANDLE(frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    return frame_graph->culled_passes_count;
}

zest_uint zest_GetFrameGraphSubmissionCount(zest_frame_graph frame_graph) {
    ZEST_ASSERT_HANDLE(frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    return zest_vec_size(frame_graph->submissions);
}

zest_uint zest_GetFrameGraphSubmissionBatchCount(zest_frame_graph frame_graph, zest_uint submission_index) {
    ZEST_ASSERT_HANDLE(frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    if (submission_index < zest_vec_size(frame_graph->submissions)) {
        return zest_vec_size(frame_graph->submissions[submission_index].batches);
    }
    return 0;
}

zest_uint zest_GetSubmissionBatchPassCount(const zest_submission_batch_t *batch) {
    return zest_vec_size(batch->pass_indices);
}

const zest_submission_batch_t *zest_GetFrameGraphSubmissionBatch(zest_frame_graph frame_graph, zest_uint submission_index, zest_uint batch_index) {
    ZEST_ASSERT_HANDLE(frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    if (submission_index < zest_vec_size(frame_graph->submissions)) {
		zest_wave_submission_t *submission = &frame_graph->submissions[submission_index];
        if (batch_index < zest_vec_size(submission->batches)) {
            return &submission->batches[batch_index];
        }
    }
    return NULL;
}

const zest_pass_group_t *zest_GetFrameGraphFinalPass(zest_frame_graph frame_graph, zest_uint pass_index) {
    ZEST_ASSERT_HANDLE(frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    if (pass_index < zest_map_size(frame_graph->final_passes)) {
        return &frame_graph->final_passes.data[pass_index];
    }
    return NULL;
}

void zest_PrintCompiledFrameGraph(zest_frame_graph frame_graph) {
    if (!ZEST_VALID_HANDLE(frame_graph)) {
        ZEST_PRINT("frame graph handle is NULL.");
        return;
    }

	zest_context context = frame_graph->command_list.context;
    ZEST_PRINT("--- Frame graph Execution Plan, Current FIF: %i ---", context->current_fif);

    if (ZEST__NOT_FLAGGED(frame_graph->flags, zest_frame_graph_is_compiled)) {
        ZEST_PRINT("frame graph is not in a compiled state");
        return;
    }

    ZEST_PRINT("Resource List: Total Resources: %u\n", zest_bucket_array_size(&frame_graph->resources));

    zest_bucket_array_foreach(resource_index, frame_graph->resources) {
		zest_resource_node resource = zest_bucket_array_get(&frame_graph->resources, zest_resource_node_t, resource_index);
        if (resource->type & zest_resource_type_buffer) {
			ZEST_PRINT("Buffer: %s - Size: %zu", resource->name, resource->buffer_desc.size);
        } else if (resource->type & zest_resource_type_image) {
            ZEST_PRINT("Image: %s - Size: %u x %u", 
                resource->name, resource->image.info.extent.width, resource->image.info.extent.height);
        } else if (resource->type == zest_resource_type_swap_chain_image) {
            ZEST_PRINT("Swapchain Image: %s", 
                resource->name);
        }
    }

    ZEST_PRINT("");
	ZEST_PRINT("Graph Wave Layout ([G]raphics, [C]ompute, [T]ransfer)");
	ZEST_PRINT("Wave Index\tG\tC\tT\tPass Count");
	zest_vec_foreach(wave_index, frame_graph->execution_waves) {
		zest_execution_wave_t *wave = &frame_graph->execution_waves[wave_index];
		char g[2] = { (wave->queue_bits & zest_queue_graphics) > 0 ? 'X' : ' ', '\0' };
		char c[2] = { (wave->queue_bits & zest_queue_compute) > 0 ? 'X' : ' ', '\0' };
		char t[2] = { (wave->queue_bits & zest_queue_transfer) > 0 ? 'X' : ' ', '\0' };
		zest_uint pass_count = zest_vec_size(wave->pass_indices);
		ZEST_PRINT("%u\t\t%s\t%s\t%s\t%u", wave_index, g, c, t, pass_count);
	}


    ZEST_PRINT("");
    ZEST_PRINT("Number of Submission Batches: %u\n", zest_vec_size(frame_graph->submissions));

    zest_vec_foreach(submission_index, frame_graph->submissions) {
        zest_wave_submission_t *wave_submission = &frame_graph->submissions[submission_index];
		ZEST_PRINT("Wave Submission Index %i:", submission_index);
        for (zest_uint queue_index = 0; queue_index != ZEST_QUEUE_COUNT; ++queue_index) {
            zest_submission_batch_t *batch = &wave_submission->batches[queue_index];
            if (!batch->magic) continue;
            if (zest_map_valid_key(context->device->queue_names, batch->queue_family_index)) {
                ZEST_PRINT("  Target Queue Family: %s - index: %u", *zest_map_at_key(context->device->queue_names, batch->queue_family_index), batch->queue_family_index);
            } else {
                ZEST_PRINT("  Target Queue Family: Ignored - index: %u", batch->queue_family_index);
            }

            // --- Print Wait Semaphores for the Batch ---
            // (Your batch struct needs to store enough info for this, e.g., an array of wait semaphores and stages)
            // For simplicity, assuming single wait_on_batch_semaphore for now, and you'd identify if it's external
            if (batch->wait_values) {
                // This stage should ideally be stored with the batch submission info by EndRenderGraph
                ZEST_PRINT("  Waits on the following Semaphores:");
                zest_vec_foreach(semaphore_index, batch->wait_values) {
                    zest_text_t pipeline_stages = zest__pipeline_stage_flags_to_string(context, batch->wait_stages[semaphore_index]);
                    if (zest_vec_size(batch->wait_values) && batch->wait_values[semaphore_index] > 0) {
                        ZEST_PRINT("     Timeline Semaphore: %p, Value: %zu at Stage: %s", context->device->platform->get_final_wait_ptr(batch, semaphore_index), batch->wait_values[semaphore_index], pipeline_stages.str);
                    } else {
                        ZEST_PRINT("     Binary Semaphore:   %p at Stage: %s", context->device->platform->get_final_wait_ptr(batch, semaphore_index), pipeline_stages.str);
                    }
                    zest_FreeText(context->device->allocator, &pipeline_stages);
                }
            } else {
                ZEST_PRINT("  Does not wait on any semaphores.");
            }

            ZEST_PRINT("  Passes in this batch:");
            zest_vec_foreach(batch_pass_index, batch->pass_indices) {
                int pass_index = batch->pass_indices[batch_pass_index];
                zest_pass_group_t *pass_node = &frame_graph->final_passes.data[pass_index];
                zest_execution_details_t *exe_details = &pass_node->execution_details;

                ZEST_PRINT("    Pass [%d] (QueueType: %d)",
                    pass_index, pass_node->compiled_queue_info.queue_type);
                zest_vec_foreach(pass_index, pass_node->passes) {
                    ZEST_PRINT("       %s", pass_node->passes[pass_index]->name);
                }

                if (zest_vec_size(exe_details->barriers.acquire_buffer_barriers) > 0 ||
                    zest_vec_size(exe_details->barriers.acquire_image_barriers) > 0) {
                    zest_text_t overal_src_pipeline_stages = zest__pipeline_stage_flags_to_string(context, exe_details->barriers.overall_src_stage_mask_for_acquire_barriers);
                    zest_text_t overal_dst_pipeline_stages = zest__pipeline_stage_flags_to_string(context, exe_details->barriers.overall_dst_stage_mask_for_acquire_barriers);
                    ZEST_PRINT("      Acquire Barriers (Overall Pipeline Src Stages: %s, Dst Stages: %s):",
                        overal_src_pipeline_stages.str,
                        overal_dst_pipeline_stages.str);
                    zest_FreeText(context->device->allocator, &overal_src_pipeline_stages);
                    zest_FreeText(context->device->allocator, &overal_dst_pipeline_stages);

					if (zest_vec_size(exe_details->barriers.acquire_image_barriers)) {
						ZEST_PRINT("        Images:");
						zest_vec_foreach(barrier_index, exe_details->barriers.acquire_image_barriers) {
							zest_image_barrier_t *imb = &exe_details->barriers.acquire_image_barriers[barrier_index];
							zest_resource_node image_resource = exe_details->barriers.acquire_image_barrier_nodes[barrier_index];
							zest_text_t src_access_mask = zest__access_flags_to_string(context, imb->src_access_mask);
							zest_text_t dst_access_mask = zest__access_flags_to_string(context, imb->dst_access_mask);
							ZEST_PRINT("            %s, Layout: %s -> %s, Access: %s -> %s, QFI: %u -> %u",
									   image_resource->name,
									   zest__image_layout_to_string(imb->old_layout), zest__image_layout_to_string(imb->new_layout),
									   src_access_mask.str, dst_access_mask.str,
									   imb->src_queue_family_index, imb->dst_queue_family_index);
							zest_FreeText(context->device->allocator, &src_access_mask);
							zest_FreeText(context->device->allocator, &dst_access_mask);
						}
					}

					if (zest_vec_size(exe_details->barriers.acquire_buffer_barriers)) {
						ZEST_PRINT("        Buffers:");
						zest_vec_foreach(barrier_index, exe_details->barriers.acquire_buffer_barriers) {
							zest_buffer_barrier_t *bmb = &exe_details->barriers.acquire_buffer_barriers[barrier_index];
							zest_resource_node buffer_resource = exe_details->barriers.acquire_buffer_barrier_nodes[barrier_index];
							// You need a robust way to get resource_name from bmb->image
							zest_text_t src_access_mask = zest__access_flags_to_string(context, bmb->src_access_mask);
							zest_text_t dst_access_mask = zest__access_flags_to_string(context, bmb->dst_access_mask);
							ZEST_PRINT("            %s | Access: %s -> %s, QFI: %u -> %u, Size: %zu",
									   buffer_resource->name,
									   src_access_mask.str, dst_access_mask.str,
									   bmb->src_queue_family_index, bmb->dst_queue_family_index,
									   buffer_resource->buffer_desc.size);
							zest_FreeText(context->device->allocator, &src_access_mask);
							zest_FreeText(context->device->allocator, &dst_access_mask);
						}
					}
                }

                // Print Inputs and Outputs (simplified)
                // ...

                if (zest_vec_size(exe_details->color_attachments)) {
                    ZEST_PRINT("      RenderArea: (%d,%d)-(%ux%u)",
                        exe_details->render_area.offset.x, exe_details->render_area.offset.y,
                        exe_details->render_area.extent.width, exe_details->render_area.extent.height);
                    // Further detail: iterate VkRenderPassCreateInfo's attachments (if stored or re-derived)
                    // and print each VkAttachmentDescription's load/store/layouts and clear values.
                }

                if (zest_vec_size(exe_details->barriers.release_buffer_barriers) > 0 ||
                    zest_vec_size(exe_details->barriers.release_image_barriers) > 0) {
                    zest_text_t overal_src_pipeline_stages = zest__pipeline_stage_flags_to_string(context, exe_details->barriers.overall_src_stage_mask_for_release_barriers);
                    zest_text_t overal_dst_pipeline_stages = zest__pipeline_stage_flags_to_string(context, exe_details->barriers.overall_dst_stage_mask_for_release_barriers);
                    ZEST_PRINT("      Release Barriers (Overall Pipeline Src Stages: %s, Dst Stages: %s):",
                        overal_src_pipeline_stages.str,
                        overal_dst_pipeline_stages.str);
                    zest_FreeText(context->device->allocator, &overal_src_pipeline_stages);
                    zest_FreeText(context->device->allocator, &overal_dst_pipeline_stages);

					if (zest_vec_size(exe_details->barriers.release_image_barriers)) {
						ZEST_PRINT("        Images:");
						zest_vec_foreach(barrier_index, exe_details->barriers.release_image_barriers) {
							zest_image_barrier_t *imb = &exe_details->barriers.release_image_barriers[barrier_index];
							zest_resource_node image_resource = exe_details->barriers.release_image_barrier_nodes[barrier_index];
							zest_text_t src_access_mask = zest__access_flags_to_string(context, imb->src_access_mask);
							zest_text_t dst_access_mask = zest__access_flags_to_string(context, imb->dst_access_mask);
							ZEST_PRINT("            %s, Layout: %s -> %s, Access: %s -> %s, QFI: %u -> %u",
									   image_resource->name,
									   zest__image_layout_to_string(imb->old_layout), zest__image_layout_to_string(imb->new_layout),
									   src_access_mask.str, dst_access_mask.str,
									   imb->src_queue_family_index, imb->dst_queue_family_index);
							zest_FreeText(context->device->allocator, &src_access_mask);
							zest_FreeText(context->device->allocator, &dst_access_mask);
						}
					}

					if (zest_vec_size(exe_details->barriers.release_buffer_barriers)) {
						ZEST_PRINT("        Buffers:");
						zest_vec_foreach(barrier_index, exe_details->barriers.release_buffer_barriers) {
							zest_buffer_barrier_t *bmb = &exe_details->barriers.release_buffer_barriers[barrier_index];
							zest_resource_node buffer_resource = exe_details->barriers.release_buffer_barrier_nodes[barrier_index];
							// You need a robust way to get resource_name from bmb->image
							zest_text_t src_access_mask = zest__access_flags_to_string(context, bmb->src_access_mask);
							zest_text_t dst_access_mask = zest__access_flags_to_string(context, bmb->dst_access_mask);
							ZEST_PRINT("            %s, Access: %s -> %s, QFI: %u -> %u, Size: %zu",
									   buffer_resource->name,
									   src_access_mask.str, dst_access_mask.str,
									   bmb->src_queue_family_index, bmb->dst_queue_family_index,
									   buffer_resource->buffer_desc.size);
							zest_FreeText(context->device->allocator, &src_access_mask);
							zest_FreeText(context->device->allocator, &dst_access_mask);
						}
					}
                }
            }

            // --- Print Signal Semaphores for the Batch ---
            if (batch->signal_values != 0) {
                ZEST_PRINT("  Signal Semaphores:");
                zest_vec_foreach(signal_index, batch->signal_values) {
                    if (batch->signal_values[signal_index] > 0) {
                        ZEST_PRINT("  Timeline Semaphore: %p, Value: %zu", context->device->platform->get_final_signal_ptr(batch, signal_index), batch->signal_values[signal_index]);
                    } else {
                        ZEST_PRINT("  Binary Semaphore: %p", context->device->platform->get_final_signal_ptr(batch, signal_index));
                    }
                }
            }
            ZEST_PRINT(""); // End of batch
        }
    }
	ZEST_PRINT("--- End of Report ---");
}

void zest_EmptyRenderPass(const zest_command_list command_list, void *user_data) {
    //Nothing here to render, it's just for frame graphs that have nothing to render
}

zest_uint zest__acquire_bindless_image_index(zest_image image, zest_image_view view, zest_set_layout layout, zest_descriptor_set set, zest_binding_number_type target_binding_number, zest_descriptor_type descriptor_type) {
    zest_uint binding_number = ZEST_INVALID;
	zest_vec_foreach(i, layout->bindings) {
		zest_descriptor_binding_desc_t *binding = &layout->bindings[i];
		if (binding->binding == target_binding_number && binding->type == descriptor_type) {
			binding_number = binding->binding;
			break;
        }
	}

    ZEST_ASSERT(binding_number != ZEST_INVALID);    //Could not find an appropriate descriptor type in the layout with that target binding number!
    zest_uint array_index = zest__acquire_bindless_index(layout, binding_number);
    if (array_index == ZEST_INVALID) {
        //Ran out of space in the descriptor pool
        ZEST__REPORT(image->handle.context, zest_report_bindless_indexes, "Ran out of space in the descriptor pool when trying to acquire an index for image, binding number %i.", binding_number);
        return ZEST_INVALID;
    }

	zest_context context = image->handle.context;
    context->device->platform->update_bindless_image_descriptor(context, binding_number, array_index, descriptor_type, image, view, 0, set);

    image->bindless_index[binding_number] = array_index;
    return array_index;
}

zest_uint zest__acquire_bindless_sampler_index(zest_sampler sampler, zest_set_layout layout, zest_descriptor_set set, zest_binding_number_type target_binding_number) {
    zest_uint binding_number = ZEST_INVALID;
    zest_descriptor_type descriptor_type;
    zest_vec_foreach(i, layout->bindings) {
        zest_descriptor_binding_desc_t *binding = &layout->bindings[i];
        if (binding->binding == target_binding_number && binding->type == zest_descriptor_type_sampler) {
            descriptor_type = binding->type;
            binding_number = binding->binding;
            break;
        }
    }

    ZEST_ASSERT(binding_number != ZEST_INVALID);    //Could not find an appropriate descriptor type in the layout with that target binding number!
    zest_uint array_index = zest__acquire_bindless_index(layout, binding_number);
	zest_context context = sampler->handle.context;
    if (array_index == ZEST_INVALID) {
        //Ran out of space in the descriptor pool
        ZEST__REPORT(context, zest_report_bindless_indexes, "Ran out of space in the descriptor pool when trying to acquire an index for image, binding number %i.", binding_number);
        return ZEST_INVALID;
    }

    context->device->platform->update_bindless_image_descriptor(context, binding_number, array_index, descriptor_type, 0, 0, sampler, set);

    return array_index;
}

zest_uint zest__acquire_bindless_storage_buffer_index(zest_buffer buffer, zest_set_layout layout, zest_descriptor_set set, zest_uint target_binding_number) {
    zest_uint binding_number = ZEST_INVALID;
    zest_vec_foreach(i, layout->bindings) {
        zest_descriptor_binding_desc_t *layout_binding = &layout->bindings[i];
        if (target_binding_number == layout_binding->binding && layout_binding->type == zest_descriptor_type_storage_buffer) {
            binding_number = layout_binding->binding;
            break;
        }
    }

    ZEST_ASSERT(binding_number != ZEST_INVALID);    //Could not find an appropriate descriptor type in the layout with that target binding number!
    zest_uint array_index = zest__acquire_bindless_index(layout, binding_number);
	zest_context context = buffer->context;
    context->device->platform->update_bindless_buffer_descriptor(binding_number, array_index, buffer, set);

    buffer->array_index = array_index;

    return array_index;
}

zest_uint zest_AcquireGlobalSampledImageIndex(zest_image_handle image_handle, zest_binding_number_type binding_number) {
    zest_image image = (zest_image)zest__get_store_resource_checked(image_handle.context, image_handle.value);
	zest_context context = image_handle.context;
    zest_uint index = zest__acquire_bindless_image_index(image, image->default_view, context->device->global_bindless_set_layout, context->device->global_set, binding_number, zest_descriptor_type_sampled_image);
    return index;
}

zest_uint zest_AcquireGlobalStorageImageIndex(zest_image_handle image_handle, zest_binding_number_type binding_number) {
    zest_image image = (zest_image)zest__get_store_resource_checked(image_handle.context, image_handle.value);
	zest_context context = image_handle.context;
    zest_uint index = zest__acquire_bindless_image_index(image, image->default_view, context->device->global_bindless_set_layout, context->device->global_set, binding_number, zest_descriptor_type_storage_image);
    return index;
}

zest_uint zest_AcquireGlobalSamplerIndex(zest_sampler_handle sampler_handle) {
    zest_sampler sampler = (zest_sampler)zest__get_store_resource_checked(sampler_handle.context, sampler_handle.value);
	zest_context context = sampler_handle.context;
    zest_uint index = zest__acquire_bindless_sampler_index(sampler, context->device->global_bindless_set_layout, context->device->global_set, zest_sampler_binding);
    return index;
}

zest_uint *zest_AcquireGlobalImageMipIndexes(zest_image_handle image_handle, zest_image_view_array_handle view_array_handle, zest_binding_number_type binding_number, zest_descriptor_type descriptor_type) {
	ZEST_ASSERT(image_handle.context == view_array_handle.context);	//image and view arrays must have the same context!
    zest_image image = (zest_image)zest__get_store_resource_checked(image_handle.context, image_handle.value);
    ZEST_ASSERT(image->info.mip_levels > 1);         //The resource does not have any mip levels. Make sure to set the number of mip levels when creating the resource in the frame graph

    if (zest_map_valid_key(image->mip_indexes, (zest_key)binding_number)) {
        zest_mip_index_collection *mip_collection = zest_map_at_key(image->mip_indexes, (zest_key)binding_number);
        return mip_collection->mip_indexes;
    }

	zest_context context = image_handle.context;

    zest_image_view_array view_array = *(zest_image_view_array*)zest__get_store_resource_checked(view_array_handle.context, view_array_handle.value);
    zest_mip_index_collection mip_collection = ZEST__ZERO_INIT(zest_mip_index_collection);
    mip_collection.binding_number = binding_number;
    zest_set_layout global_layout = context->device->global_bindless_set_layout;
	zest_sampler sampler = 0;
    for (int mip_index = 0; mip_index != view_array->count; ++mip_index) {
        zest_uint bindless_index = zest__acquire_bindless_index(global_layout, binding_number);
        context->device->platform->update_bindless_image_descriptor(context, binding_number, bindless_index, descriptor_type, image, &view_array->views[mip_index], sampler, context->device->global_set);
        zest_vec_push(image_handle.context->device->allocator, mip_collection.mip_indexes, bindless_index);
    }
    zest_map_insert_key(image_handle.context->device->allocator, image->mip_indexes, (zest_key)binding_number, mip_collection);
    zest_uint size = zest_map_size(image->mip_indexes);
    return mip_collection.mip_indexes;
}

zest_uint zest_AcquireGlobalStorageBufferIndex(zest_buffer buffer) {
	ZEST_ASSERT_BUFFER_HANDLE(buffer);	//Not a valid buffer handle
	zest_context context = buffer->context;
    return zest__acquire_bindless_storage_buffer_index(buffer, context->device->global_bindless_set_layout, context->device->global_set, zest_storage_buffer_binding);
}

void zest_AcquireGlobalInstanceLayerBufferIndex(zest_layer_handle layer_handle) {
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(layer_handle.context, layer_handle.value);
	zest_context context = layer_handle.context;
    ZEST_ASSERT(ZEST__FLAGGED(layer->flags, zest_layer_flag_manual_fif));   //Layer must have been created with a persistant vertex buffer
    zest_ForEachFrameInFlight(fif) {
        zest_AcquireGlobalStorageBufferIndex(layer->memory_refs[fif].device_vertex_data);
    }
    layer->bindless_set = context->device->global_set;
    ZEST__FLAG(layer->flags, zest_layer_flag_using_global_bindless_layout);
}

void zest_ReleaseGlobalStorageBufferIndex(zest_buffer buffer) {
	ZEST_ASSERT_BUFFER_HANDLE(buffer);
    ZEST_ASSERT(buffer->array_index != ZEST_INVALID);
	zest_context context = buffer->context;
    zest__release_bindless_index(context->device->global_bindless_set_layout, zest_storage_buffer_binding, buffer->array_index);
}

void zest_ReleaseGlobalImageIndex(zest_image_handle handle, zest_binding_number_type binding_number) {
    zest_image image = (zest_image)zest__get_store_resource_checked(handle.context, handle.value);
	zest_context context = handle.context;
    ZEST_ASSERT(binding_number < zest_max_global_binding_number);
    if (image->bindless_index[binding_number] != ZEST_INVALID) {
        zest__release_bindless_index(context->device->global_bindless_set_layout, binding_number, image->bindless_index[binding_number]);
    }
}

void zest_ReleaseAllGlobalImageIndexes(zest_image_handle handle) {
    zest_image image = (zest_image)zest__get_store_resource_checked(handle.context, handle.value);
    zest__release_all_global_texture_indexes(image);
}

void zest_ReleaseGlobalBindlessIndex(zest_context context, zest_uint index, zest_binding_number_type binding_number) {
    ZEST_ASSERT(index != ZEST_INVALID);
    zest__release_bindless_index(context->device->global_bindless_set_layout, binding_number, index);
}

zest_set_layout_handle zest_GetGlobalBindlessLayout(zest_context context) {
    return context->device->global_bindless_set_layout_handle;
}

zest_descriptor_set zest_GetGlobalBindlessSet(zest_context context) {
    return context->device->global_set;
}

zest_resource_node zest__add_transient_image_resource(const char *name, const zest_image_info_t *description, zest_bool assign_bindless, zest_bool image_view_binding_only) {
    ZEST_ASSERT_HANDLE(zest__frame_graph_builder->frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    zest_frame_graph frame_graph = zest__frame_graph_builder->frame_graph;
    zest_resource_node_t resource = ZEST__ZERO_INIT(zest_resource_node_t);
	zest_context context = zest__frame_graph_builder->context;
    resource.name = name;
	resource.id = frame_graph->id_counter++;
    resource.first_usage_pass_idx = ZEST_INVALID;
    resource.image.info = *description;
	resource.type = description->format == context->device->depth_format ? zest_resource_type_depth : zest_resource_type_image;
    resource.frame_graph = frame_graph;
    resource.current_queue_family_index = ZEST_QUEUE_FAMILY_IGNORED;
    resource.magic = zest_INIT_MAGIC(zest_struct_type_resource_node);
    resource.producer_pass_idx = -1;
    resource.image.backend = (zest_image_backend)context->device->platform->new_frame_graph_image_backend(context->frame_graph_allocator[context->current_fif], &resource.image, NULL);
	ZEST__FLAG(resource.flags, zest_resource_node_flag_transient);
    zest_resource_node node = zest__add_frame_graph_resource(&resource);
    node->image_layout = zest_image_layout_undefined;
    return node;
}

void zest_FlagResourceAsEssential(zest_resource_node resource) {
    ZEST_ASSERT_HANDLE(resource);   //Not a valid resource handle!
    ZEST__FLAG(resource->flags, zest_resource_node_flag_essential_output);
}

void zest_AddSwapchainToRenderTargetGroup(zest_output_group group) {
    ZEST_ASSERT_HANDLE(zest__frame_graph_builder->frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    zest_frame_graph frame_graph = zest__frame_graph_builder->frame_graph;
    ZEST_ASSERT_HANDLE(frame_graph->swapchain_resource);    //frame graph must have a swapchain, use zest_ImportSwapchainResource to import the context swapchain to the render graph
    ZEST_ASSERT_HANDLE(group);                      //Not a valid render target group
	zest_context context = zest__frame_graph_builder->context;
    zest_vec_linear_push(context->frame_graph_allocator[context->current_fif], group->resources, frame_graph->swapchain_resource);
}

void zest_AddImageToRenderTargetGroup(zest_output_group group, zest_resource_node image) {
    ZEST_ASSERT_HANDLE(zest__frame_graph_builder->frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    zest_frame_graph frame_graph = zest__frame_graph_builder->frame_graph;
    ZEST_ASSERT_HANDLE(group);                      //Not a valid render target group
    ZEST_ASSERT(image->type & zest_resource_type_is_image_or_depth);  //Must be a depth buffer resource type
	zest_FlagResourceAsEssential(image);
	zest_context context = zest__frame_graph_builder->context;
    zest_vec_linear_push(context->frame_graph_allocator[context->current_fif], group->resources, image);
}

zest_output_group zest_CreateOutputGroup() {
    ZEST_ASSERT_HANDLE(zest__frame_graph_builder->frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
	zest_context context = zest__frame_graph_builder->context;
    zest_frame_graph frame_graph = zest__frame_graph_builder->frame_graph;
    zest_output_group group = (zest_output_group)zloc_LinearAllocation(context->frame_graph_allocator[context->current_fif], sizeof(zest_output_group_t));
    *group = ZEST__ZERO_INIT(zest_output_group_t);
    group->magic = zest_INIT_MAGIC(zest_struct_type_render_target_group);
    return group;
}

zest_resource_node zest_AddTransientImageResource(const char *name, zest_image_resource_info_t *info) {
    ZEST_ASSERT_HANDLE(zest__frame_graph_builder->frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    zest_frame_graph frame_graph = zest__frame_graph_builder->frame_graph;
	zest_context context = zest__frame_graph_builder->context;
    zest_image_info_t description = ZEST__ZERO_INIT(zest_image_info_t);
    description.extent.width = info->width ? info->width : zest_ScreenWidth(context);
    description.extent.height = info->height ? info->height : zest_ScreenHeight(context);
    description.sample_count = (info->usage_hints & zest_resource_usage_hint_msaa) ? context->device->platform->get_msaa_sample_count(context) : zest_sample_count_1_bit;
    description.mip_levels = info->mip_levels ? info->mip_levels : 1;
    description.layer_count = info->layer_count ? info->layer_count : 1;
	description.format = info->format == zest_format_depth ? context->device->depth_format : info->format;
    description.aspect_flags = zest__determine_aspect_flag(description.format);
	zest_resource_node resource = zest__add_transient_image_resource(name, &description, 0, ZEST_TRUE);
    zest__interpret_hints(resource, info->usage_hints);
    return resource;
}

zest_resource_node zest_AddTransientBufferResource(const char *name, const zest_buffer_resource_info_t *info) {
    ZEST_ASSERT_HANDLE(zest__frame_graph_builder->frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    zest_frame_graph frame_graph = zest__frame_graph_builder->frame_graph;
	zest_context context = zest__frame_graph_builder->context;
    zest_resource_node_t node = ZEST__ZERO_INIT(zest_resource_node_t);
    node.name = name;
    node.id = frame_graph->id_counter++;
    node.first_usage_pass_idx = ZEST_INVALID;
    node.buffer_desc.size = info->size;
    node.type = zest_resource_type_buffer;
	zest_memory_usage usage = info->usage_hints & zest_resource_usage_hint_cpu_transfer ? zest_memory_usage_cpu_to_gpu : zest_memory_usage_gpu_only;
    if (info->usage_hints & zest_resource_usage_hint_vertex_buffer) {
        node.buffer_desc.buffer_info = zest_CreateBufferInfo(zest_buffer_type_vertex, usage);
		ZEST__FLAG(node.type, zest_resource_type_vertex_buffer);
    } else if (info->usage_hints & zest_resource_usage_hint_index_buffer) {
        node.buffer_desc.buffer_info = zest_CreateBufferInfo(zest_buffer_type_index, usage);
		ZEST__FLAG(node.type, zest_resource_type_index_buffer);
    } else {
        node.buffer_desc.buffer_info = zest_CreateBufferInfo(zest_buffer_type_storage, usage);
    }
    if (info->usage_hints & zest_resource_usage_hint_copy_src) {
        node.buffer_desc.buffer_info.buffer_usage_flags |= zest_buffer_usage_transfer_src_bit;
    }
    node.frame_graph = frame_graph;
	node.buffer_desc.buffer_info.flags = zest_memory_pool_flag_single_buffer;
    node.current_queue_family_index = ZEST_QUEUE_FAMILY_IGNORED;
    node.magic = zest_INIT_MAGIC(zest_struct_type_resource_node);
    node.producer_pass_idx = -1;
    ZEST__FLAG(node.flags, zest_resource_node_flag_transient);
    return zest__add_frame_graph_resource(&node);
}

zest_resource_node zest_AddTransientLayerResource(const char *name, const zest_layer_handle layer_handle, zest_bool prev_fif) {
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(layer_handle.context, layer_handle.value);
    zest_size layer_size = layer->memory_refs[layer->fif].instance_count * layer->instance_struct_size;
    ZEST_ASSERT_HANDLE(zest__frame_graph_builder->frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    zest_frame_graph frame_graph = zest__frame_graph_builder->frame_graph;
    ZEST__MAYBE_FLAG(layer->flags, zest_layer_flag_use_prev_fif, prev_fif);
    zest_resource_node resource = 0;
	zest_context context = layer_handle.context;
    if (ZEST__NOT_FLAGGED(layer->flags, zest_layer_flag_manual_fif)) {
        zest_buffer_resource_info_t buffer_desc = ZEST__ZERO_INIT(zest_buffer_resource_info_t);
        buffer_desc.size = layer_size;
        buffer_desc.usage_hints = zest_resource_usage_hint_vertex_buffer;
        resource = zest_AddTransientBufferResource(name, &buffer_desc);
    } else {
        zest_uint fif = prev_fif ? layer->prev_fif : layer->fif;
        zest_buffer buffer = layer->memory_refs[fif].device_vertex_data;
		zest_resource_node_t node = zest__create_import_buffer_resource_node(name, buffer);
		resource = zest__add_frame_graph_resource(&node);
		resource->access_mask = context->device->platform->get_buffer_last_access_mask(buffer);
        resource->bindless_index[0] = layer->memory_refs[fif].device_vertex_data->array_index;
    }
    resource->user_data = layer;
    resource->buffer_provider = zest__instance_layer_resource_provider;
    layer->vertex_buffer_node = resource;
    return resource;
}

zest_resource_node_t zest__create_import_buffer_resource_node(const char *name, zest_buffer buffer) {
    ZEST_ASSERT_HANDLE(zest__frame_graph_builder->frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    zest_frame_graph frame_graph = zest__frame_graph_builder->frame_graph;
    zest_resource_node_t node = ZEST__ZERO_INIT(zest_resource_node_t);
    node.name = name;
	node.id = frame_graph->id_counter++;
    node.first_usage_pass_idx = ZEST_INVALID;
	node.type = zest_resource_type_buffer;
	if (buffer->buffer_allocator->buffer_info.buffer_usage_flags & zest_buffer_usage_index_buffer_bit) {
		ZEST__FLAG(node.type, zest_resource_type_index_buffer);
	} else if (buffer->buffer_allocator->buffer_info.buffer_usage_flags & zest_buffer_usage_vertex_buffer_bit) {
		ZEST__FLAG(node.type, zest_resource_type_vertex_buffer);
	}
    node.frame_graph = frame_graph;
    node.magic = zest_INIT_MAGIC(zest_struct_type_resource_node);
    node.storage_buffer = buffer;
    node.current_queue_family_index = buffer->owner_queue_family;
    node.producer_pass_idx = -1;
	ZEST__FLAG(node.flags, zest_resource_node_flag_imported);
	ZEST__FLAG(node.flags, zest_resource_node_flag_essential_output);
    return node;
}

zest_resource_node_t zest__create_import_image_resource_node(const char *name, zest_image image) {
    ZEST_ASSERT_HANDLE(zest__frame_graph_builder->frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    zest_frame_graph frame_graph = zest__frame_graph_builder->frame_graph;
    zest_resource_node_t node = ZEST__ZERO_INIT(zest_resource_node_t);
    node.name = name;
	node.id = frame_graph->id_counter++;
    node.first_usage_pass_idx = ZEST_INVALID;
	node.type = zest_resource_type_image;
    node.frame_graph = frame_graph;
    node.magic = zest_INIT_MAGIC(zest_struct_type_resource_node);
    node.image = *image;
    node.view = image->default_view;
    node.current_queue_family_index = ZEST_QUEUE_FAMILY_IGNORED;
    node.producer_pass_idx = -1;
	ZEST__FLAG(node.flags, zest_resource_node_flag_imported);
	ZEST__FLAG(node.flags, zest_resource_node_flag_essential_output);
    return node;
}

zest_resource_node zest_ImportImageResource(const char *name, zest_image_handle handle, zest_resource_image_provider provider) {
    zest_image image = (zest_image)zest__get_store_resource_checked(handle.context, handle.value);
    ZEST_ASSERT_HANDLE(zest__frame_graph_builder->frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    zest_frame_graph frame_graph = zest__frame_graph_builder->frame_graph;
    zest_resource_node_t resource = zest__create_import_image_resource_node(name, image);
	ZEST__FLAG(resource.flags, zest_resource_node_flag_imported);
    resource.image_provider = provider;
    zest_resource_node node = zest__add_frame_graph_resource(&resource);
    node->image_layout = image->info.layout;
    node->linked_layout = &image->info.layout;
    return node;
}

zest_resource_node zest_ImportBufferResource(const char *name, zest_buffer buffer, zest_resource_buffer_provider provider) {
    ZEST_ASSERT_HANDLE(zest__frame_graph_builder->frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    zest_frame_graph frame_graph = zest__frame_graph_builder->frame_graph;
	zest_context context = zest__frame_graph_builder->context;
    zest_resource_node_t resource = zest__create_import_buffer_resource_node(name, buffer);
    resource.buffer_provider = provider;
    zest_resource_node node = zest__add_frame_graph_resource(&resource);
    node->access_mask = context->device->platform->get_buffer_last_access_mask(buffer);
    return node;
}

zest_resource_node zest_ImportSwapchainResource() {
    ZEST_ASSERT_HANDLE(zest__frame_graph_builder->frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
    zest_frame_graph frame_graph = zest__frame_graph_builder->frame_graph;
	zest_context context = zest__frame_graph_builder->context;
	zest_swapchain swapchain = context->swapchain;
    zest__frame_graph_builder->frame_graph->swapchain = swapchain;
    if (ZEST__NOT_FLAGGED(context->flags, zest_context_flag_swap_chain_was_acquired)) {
        ZEST_PRINT("WARNING: Swap chain is being imported but no swap chain image has been acquired. Make sure that you call zest_BeginFrame() to make sure that a swapchain image is acquired.");
    }
    zest_resource_node_t node = ZEST__ZERO_INIT(zest_resource_node_t);
    node.swapchain = swapchain;
    node.name = swapchain->name;
	node.id = frame_graph->id_counter++;
    node.first_usage_pass_idx = ZEST_INVALID;
	node.type = zest_resource_type_swap_chain_image;
    node.frame_graph = frame_graph;
    node.magic = zest_INIT_MAGIC(zest_struct_type_resource_node);
    node.image.backend = (zest_image_backend)context->device->platform->new_frame_graph_image_backend(context->frame_graph_allocator[context->current_fif], &node.image, &swapchain->images[swapchain->current_image_frame]);
    node.image.info = swapchain->images[0].info;
    node.view = swapchain->views[swapchain->current_image_frame];
    node.current_queue_family_index = ZEST_QUEUE_FAMILY_IGNORED;
    node.producer_pass_idx = -1;
    node.image_provider = zest__swapchain_resource_provider;
	ZEST__FLAG(node.flags, zest_resource_node_flag_imported);
	ZEST__FLAG(node.flags, zest_resource_node_flag_essential_output);
    frame_graph->swapchain_resource = zest__add_frame_graph_resource(&node);
    frame_graph->swapchain_resource->image_layout = zest_image_layout_undefined;
    frame_graph->swapchain_resource->last_stage_mask = zest_pipeline_stage_color_attachment_output_bit;
	ZEST__FLAG(frame_graph->flags, zest_frame_graph_expecting_swap_chain_usage);
    return frame_graph->swapchain_resource;
}

void zest_SetPassTask(zest_rg_execution_callback callback, void *user_data) {
	zest_context context = zest__frame_graph_builder->context;
    ZEST_ASSERT_HANDLE(zest__frame_graph_builder->current_pass);        //No current pass found, make sure you call zest_BeginPass
    zest_pass_node pass = zest__frame_graph_builder->current_pass;
    zest_pass_execution_callback_t callback_data;
    callback_data.callback = callback;
    callback_data.user_data = user_data;
    pass->execution_callback = callback_data;
}

void zest_SetPassInstanceLayerUpload(zest_layer_handle layer_handle) {
	zest_context context = zest__frame_graph_builder->context;
    ZEST_ASSERT_HANDLE(zest__frame_graph_builder->current_pass);        //No current pass found, make sure you call zest_BeginPass
    zest_pass_node pass = zest__frame_graph_builder->current_pass;
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(layer_handle.context, layer_handle.value);
    if (ZEST__FLAGGED(layer->flags, zest_layer_flag_manual_fif) && layer->dirty[layer->fif] == 0) {
        return;
    }
    zest_pass_execution_callback_t callback_data;
    callback_data.callback = zest_UploadInstanceLayerData;
    callback_data.user_data = (void*)(uintptr_t)layer_handle.value;
    pass->execution_callback = callback_data;
}

void zest_SetPassInstanceLayer(zest_layer_handle layer_handle) {
	zest_context context = zest__frame_graph_builder->context;
    ZEST_ASSERT_HANDLE(zest__frame_graph_builder->current_pass);        //No current pass found, make sure you call zest_BeginPass
    zest_pass_node pass = zest__frame_graph_builder->current_pass;
    zest_pass_execution_callback_t callback_data;
    callback_data.callback = zest_DrawInstanceLayer;
    callback_data.user_data = (void*)(uintptr_t)layer_handle.value;
    pass->execution_callback = callback_data;
}

zest_pass_node zest__add_pass_node(const char *name, zest_device_queue_type queue_type) {
    zest_frame_graph frame_graph = zest__frame_graph_builder->frame_graph;
    ZEST_ASSERT_HANDLE(frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
	zest_context context = zest__frame_graph_builder->context;
    zest_pass_node_t node = ZEST__ZERO_INIT(zest_pass_node_t);
    node.id = frame_graph->id_counter++;
    node.queue_info.queue_type = queue_type;
    node.name = name;
    node.magic = zest_INIT_MAGIC(zest_struct_type_pass_node);
    node.output_key = queue_type;
    switch (queue_type) {
    case zest_queue_graphics:
        node.queue_info.timeline_wait_stage = zest_pipeline_stage_vertex_input_bit | zest_pipeline_stage_vertex_shader_bit;
        node.type = zest_pass_type_graphics;
        break;
    case zest_queue_compute:
        node.queue_info.timeline_wait_stage = zest_pipeline_stage_compute_shader_bit;
        node.type = zest_pass_type_compute;
        break;
    case zest_queue_transfer:
        node.queue_info.timeline_wait_stage = zest_pipeline_stage_transfer_bit;
        node.type = zest_pass_type_transfer;
        break;
    }
    zest_pass_node pass_node = zest_bucket_array_linear_add(context->frame_graph_allocator[context->current_fif], &frame_graph->potential_passes, zest_pass_node_t);
    *pass_node = node;
    return pass_node;
}

zest_resource_node zest__add_frame_graph_resource(zest_resource_node resource) {
    zest_frame_graph frame_graph = zest__frame_graph_builder->frame_graph;
    ZEST_ASSERT_HANDLE(frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
	zest_context context = zest__frame_graph_builder->context;
    zloc_linear_allocator_t *allocator = context->frame_graph_allocator[context->current_fif];
    zest_resource_node node = zest_bucket_array_linear_add(allocator, &frame_graph->resources, zest_resource_node_t);
    *node = *resource;
    for (int i = 0; i != zest_max_global_binding_number; ++i) {
        node->bindless_index[i] = ZEST_INVALID;
    }
    zest_resource_versions_t resource_version = ZEST__ZERO_INIT(zest_resource_versions_t);
    node->version = 0;
    node->original_id = node->id;
    zest_vec_linear_push(allocator, resource_version.resources, node);
    zest_map_insert_linear_key(allocator, frame_graph->resource_versions, resource->id, resource_version);
    return node;
}

zest_resource_versions_t *zest__maybe_add_resource_version(zest_resource_node resource) {
    zest_frame_graph frame_graph = zest__frame_graph_builder->frame_graph;
    ZEST_ASSERT_HANDLE(frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
	zest_context context = zest__frame_graph_builder->context;
    zest_resource_versions_t *versions = zest_map_at_key(frame_graph->resource_versions, resource->original_id);
    zest_resource_node last_version = zest_vec_back(versions->resources);
    if (ZEST__FLAGGED(last_version->flags, zest_resource_node_flag_has_producer)) {
		zest_resource_node_t new_resource = ZEST__ZERO_INIT(zest_resource_node_t);
		new_resource.magic = zest_INIT_MAGIC(zest_struct_type_resource_node);
		new_resource.name = last_version->name;
		new_resource.aliased_resource = versions->resources[0];
		new_resource.image.info = resource->image.info;
		new_resource.buffer_desc = resource->buffer_desc;
		new_resource.id = frame_graph->id_counter++;
        new_resource.version = last_version->version + 1;
        new_resource.original_id = last_version->original_id;
        new_resource.producer_pass_idx = -1;
		ZEST__FLAG(new_resource.flags, zest_resource_node_flag_aliased);
        ZEST__FLAG(new_resource.flags, zest_resource_node_flag_has_producer);
		zloc_linear_allocator_t *allocator = context->frame_graph_allocator[context->current_fif];
		zest_resource_node node = zest_bucket_array_linear_add(allocator, &frame_graph->resources, zest_resource_node_t);
        *node = new_resource;
        zest_vec_linear_push(allocator, versions->resources, node);
    }
	ZEST__FLAG(resource->flags, zest_resource_node_flag_has_producer);
    return versions;
}

zest_pass_node zest_BeginGraphicBlankScreen(const char *name) {
	zest_context context = zest__frame_graph_builder->context;
    zest_pass_node pass = zest__add_pass_node(name, zest_queue_graphics);
    zest__frame_graph_builder->current_pass = pass;
    zest_SetPassTask(zest_EmptyRenderPass, 0);
    return pass;
}

zest_pass_node zest_BeginRenderPass(const char *name) {
    ZEST_ASSERT_HANDLE(zest__frame_graph_builder->frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
	zest_context context = zest__frame_graph_builder->context;
    ZEST_ASSERT(!zest__frame_graph_builder->current_pass, "Already begun a pass. Make sure that you call zest_EndPass before starting a new one");   
    zest_pass_node node = zest__add_pass_node(name, zest_queue_graphics);
    node->queue_info.timeline_wait_stage = zest_pipeline_stage_vertex_input_bit | zest_pipeline_stage_vertex_shader_bit;
    zest__frame_graph_builder->current_pass = node;
    return node;
}

zest_pass_node zest_BeginComputePass(zest_compute_handle compute_handle, const char *name) {
    ZEST_ASSERT_HANDLE(zest__frame_graph_builder->frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
	zest_context context = zest__frame_graph_builder->context;
    ZEST_ASSERT(!zest__frame_graph_builder->current_pass);   //Already begun a pass. Make sure that you call zest_EndPass before starting a new one
    zest_pass_node node = zest__add_pass_node(name, zest_queue_compute);
    zest_compute compute = (zest_compute)zest__get_store_resource_checked(compute_handle.context, compute_handle.value);
    node->compute = compute;
    node->queue_info.timeline_wait_stage = zest_pipeline_stage_compute_shader_bit;
    zest__frame_graph_builder->current_pass = node;
    return node;
}

zest_pass_node zest_BeginTransferPass(const char *name) {
    ZEST_ASSERT_HANDLE(zest__frame_graph_builder->frame_graph);        //Not a valid frame graph! Make sure you called BeginRenderGraph or BeginRenderToScreen
	zest_context context = zest__frame_graph_builder->context;
    ZEST_ASSERT(!zest__frame_graph_builder->current_pass);   //Already begun a pass. Make sure that you call zest_EndPass before starting a new one
    zest_pass_node node = zest__add_pass_node(name, zest_queue_transfer);
    node->queue_info.timeline_wait_stage = zest_pipeline_stage_transfer_bit;
    zest__frame_graph_builder->current_pass = node;
    return node;
}

void zest_EndPass() {
	zest_context context = zest__frame_graph_builder->context;
    ZEST_ASSERT_HANDLE(zest__frame_graph_builder->current_pass); //No begin pass found, make sure you call BeginRender/Compute/TransferPass
    zest__frame_graph_builder->current_pass = 0;
}

zest_resource_node zest_GetPassInputResource(const zest_command_list command_list, const char *name) {
    ZEST_ASSERT(zest_map_valid_name(command_list->pass_node->inputs, name));  //Not a valid input resource name. Check the name and also maybe you meant to get from outputs?
    zest_resource_usage_t *usage = zest_map_at(command_list->pass_node->inputs, name);
    return ZEST_VALID_HANDLE(usage->resource_node->aliased_resource) ? usage->resource_node->aliased_resource : usage->resource_node;
}

zest_buffer zest_GetPassInputBuffer(const zest_command_list command_list, const char *name) {
    ZEST_ASSERT(zest_map_valid_name(command_list->pass_node->inputs, name));  //Not a valid input resource name. Check the name and also maybe you meant to get from outputs?
    zest_resource_usage_t *usage = zest_map_at(command_list->pass_node->inputs, name);
    zest_resource_node resource = ZEST_VALID_HANDLE(usage->resource_node->aliased_resource) ? usage->resource_node->aliased_resource : usage->resource_node;
    return resource->storage_buffer;
}

zest_buffer zest_GetPassOutputBuffer(const zest_command_list command_list, const char *name) {
    ZEST_ASSERT(zest_map_valid_name(command_list->pass_node->outputs, name));  //Not a valid input resource name. Check the name and also maybe you meant to get from inputs?
    zest_resource_usage_t *usage = zest_map_at(command_list->pass_node->outputs, name);
    zest_resource_node resource = ZEST_VALID_HANDLE(usage->resource_node->aliased_resource) ? usage->resource_node->aliased_resource : usage->resource_node;
    return resource->storage_buffer;
}

zest_resource_node zest_GetPassOutputResource(const zest_command_list command_list, const char *name) {
    ZEST_ASSERT(zest_map_valid_name(command_list->pass_node->outputs, name));  //Not a valid output resource name. Check the name and also maybe you meant to get from inputs?
    zest_resource_usage_t *usage = zest_map_at(command_list->pass_node->outputs, name);
    return ZEST_VALID_HANDLE(usage->resource_node->aliased_resource) ? usage->resource_node->aliased_resource : usage->resource_node;
}

zest_uint zest_GetTransientSampledImageBindlessIndex(const zest_command_list command_list, zest_resource_node resource, zest_binding_number_type binding_number) {
    ZEST_ASSERT_HANDLE(resource);            // Not a valid resource handle
    ZEST_ASSERT(resource->type & zest_resource_type_is_image);  //Must be an image resource type
	zest_context context = zest__frame_graph_builder->context;
    zest_frame_graph frame_graph = command_list->frame_graph;
    if (resource->bindless_index[binding_number] != ZEST_INVALID) return resource->bindless_index[binding_number];
    zest_set_layout bindless_layout = frame_graph->bindless_layout;
    zest_uint bindless_index = zest__acquire_bindless_index(bindless_layout, binding_number);

	zest_descriptor_type descriptor_type = binding_number == zest_storage_image_binding ? zest_descriptor_type_storage_image : zest_descriptor_type_sampled_image;
    context->device->platform->update_bindless_image_descriptor(context, binding_number, bindless_index, descriptor_type, &resource->image, resource->view, 0, frame_graph->bindless_set);

    zest_binding_index_for_release_t binding_index = { frame_graph->bindless_layout, bindless_index, (zest_uint)binding_number };
    zest_vec_push(context->device->allocator, context->device->deferred_resource_freeing_list.binding_indexes[context->current_fif], binding_index);

    resource->bindless_index[binding_number];
    return bindless_index;
}

zest_uint *zest_GetTransientSampledMipBindlessIndexes(const zest_command_list command_list, zest_resource_node resource, zest_binding_number_type binding_number) {
    zest_set_layout bindless_layout = command_list->frame_graph->bindless_layout;
    ZEST_ASSERT(resource->type & zest_resource_type_is_image);  //Must be an image resource type
    ZEST_ASSERT(resource->image.info.mip_levels > 1);   //The resource does not have any mip levels. Make sure to set the number of mip levels when creating the resource in the frame graph
    ZEST_ASSERT(resource->current_state_index < zest_vec_size(resource->journey));
    zest_frame_graph frame_graph = command_list->frame_graph;
	zest_context context = zest__frame_graph_builder->context;

    if (zest_map_valid_key(resource->image.mip_indexes, (zest_key)binding_number)) {
        zest_mip_index_collection *mip_collection = zest_map_at_key(resource->image.mip_indexes, (zest_key)binding_number);
        return mip_collection->mip_indexes;
    }
    zloc_linear_allocator_t *allocator = context->frame_graph_allocator[context->current_fif];
    if (!resource->view_array) {
        resource->view_array = context->device->platform->create_image_views_per_mip(&resource->image, zest_image_view_type_2d, 0, resource->image.info.layer_count, allocator);
		resource->view_array->handle.context = context;
    }
	zest_descriptor_type descriptor_type = binding_number == zest_storage_image_binding ? zest_descriptor_type_storage_image : zest_descriptor_type_sampled_image;
    zest_mip_index_collection mip_collection = ZEST__ZERO_INIT(zest_mip_index_collection);
	for (int mip_index = 0; mip_index != resource->view_array->count; ++mip_index) {
		zest_uint bindless_index = zest__acquire_bindless_index(bindless_layout, binding_number);
		zest_vec_linear_push(allocator, resource->mip_level_bindless_indexes, bindless_index);

        context->device->platform->update_bindless_image_descriptor(context, binding_number, bindless_index, descriptor_type, &resource->image, &resource->view_array->views[mip_index], 0, frame_graph->bindless_set);

		zest_binding_index_for_release_t mip_binding_index = { frame_graph->bindless_layout, bindless_index, (zest_uint)binding_number };
		zest_vec_push(context->device->allocator, context->device->deferred_resource_freeing_list.binding_indexes[context->current_fif], mip_binding_index);
		zest_vec_linear_push(allocator, mip_collection.mip_indexes, bindless_index );
	}
    zest_map_insert_linear_key(allocator, resource->image.mip_indexes, (zest_key)binding_number, mip_collection);
    return mip_collection.mip_indexes;
}

zest_uint zest_GetTransientBufferBindlessIndex(const zest_command_list command_list, zest_resource_node resource) {
    zest_set_layout bindless_layout = command_list->frame_graph->bindless_layout;
    ZEST_ASSERT_HANDLE(resource);   //Not a valid resource handle
    ZEST_ASSERT(resource->type & zest_resource_type_buffer);   //Must be a buffer resource type for this bindlesss index acquisition
	zest_context context = zest__frame_graph_builder->context;
    if (resource->bindless_index[0] != ZEST_INVALID) return resource->bindless_index[0];
    zest_frame_graph frame_graph = command_list->frame_graph;
	zest_uint bindless_index = zest__acquire_bindless_index(bindless_layout, zest_storage_buffer_binding);

    context->device->platform->update_bindless_buffer_descriptor(zest_storage_buffer_binding, bindless_index, resource->storage_buffer, frame_graph->bindless_set);

	zest_binding_index_for_release_t binding_index = { frame_graph->bindless_layout, bindless_index, zest_storage_buffer_binding };
	zest_vec_push(context->device->allocator, context->device->deferred_resource_freeing_list.binding_indexes[context->current_fif], binding_index);
    resource->bindless_index[0] = bindless_index;
    return bindless_index;
}

zest_uint zest_GetResourceMipLevels(zest_resource_node resource) {
    ZEST_ASSERT_HANDLE(resource);	//Not a valid resource handle!
    return resource->image.info.mip_levels;
}

zest_uint zest_GetResourceWidth(zest_resource_node resource) {
    ZEST_ASSERT_HANDLE(resource);	//Not a valid resource handle!
    return resource->image.info.extent.width;
}

zest_uint zest_GetResourceHeight(zest_resource_node resource) {
    ZEST_ASSERT_HANDLE(resource);	//Not a valid resource handle!
    return resource->image.info.extent.height;
}

zest_image zest_GetResourceImage(zest_resource_node resource) {
	ZEST_ASSERT_HANDLE(resource);   //Not a valid resource handle!
	if (resource->type & zest_resource_type_is_image_or_depth) {
        return &resource->image;
	}
	return NULL;
}

zest_resource_type zest_GetResourceType(zest_resource_node resource_node) {
	ZEST_ASSERT_HANDLE(resource_node);   //Not a valid resource handle!
	return resource_node->type;
}

zest_image_info_t zest_GetResourceImageDescription(zest_resource_node resource_node) {
	ZEST_ASSERT_HANDLE(resource_node);   //Not a valid resource handle!
	return resource_node->image.info;
}

void *zest_GetResourceUserData(zest_resource_node resource_node) {
	ZEST_ASSERT_HANDLE(resource_node);	 //Not a valid resource handle!
	return resource_node->user_data;
}

void zest_SetResourceUserData(zest_resource_node resource_node, void *user_data) {
	ZEST_ASSERT_HANDLE(resource_node);	 //Not a valid resource handle!
	resource_node->user_data = user_data;
}

void zest_SetResourceClearColor(zest_resource_node resource, float red, float green, float blue, float alpha) {
    ZEST_ASSERT_HANDLE(resource);   //Not a valid resource handle!
    resource->clear_color = ZEST_STRUCT_LITERAL(zest_vec4, red, green, blue, alpha);
}

void zest__add_pass_buffer_usage(zest_pass_node pass_node, zest_resource_node resource, zest_resource_purpose purpose, zest_pipeline_stage_flags relevant_pipeline_stages, zest_bool is_output) {
    zest_resource_usage_t usage = ZEST__ZERO_INIT(zest_resource_usage_t);    
    usage.resource_node = resource;
    usage.purpose = purpose;

    switch (purpose) {
    case zest_purpose_vertex_buffer:
        usage.access_mask = zest_access_vertex_attribute_read_bit;
        usage.stage_mask = zest_pipeline_stage_vertex_input_bit;
        resource->buffer_desc.buffer_info.buffer_usage_flags |= zest_buffer_usage_vertex_buffer_bit;
        break;
    case zest_purpose_index_buffer:
        usage.access_mask = zest_access_index_read_bit;
        usage.stage_mask = zest_pipeline_stage_vertex_input_bit;
        resource->buffer_desc.buffer_info.buffer_usage_flags |= zest_buffer_usage_index_buffer_bit;
        break;
    case zest_purpose_uniform_buffer:
        usage.access_mask = zest_access_uniform_read_bit;
        usage.stage_mask = relevant_pipeline_stages; 
        resource->buffer_desc.buffer_info.buffer_usage_flags |= zest_buffer_usage_uniform_buffer_bit;
        break;
    case zest_purpose_storage_buffer_read:
        usage.access_mask = zest_access_shader_read_bit;
        usage.stage_mask = relevant_pipeline_stages;
        resource->buffer_desc.buffer_info.buffer_usage_flags |= zest_buffer_usage_storage_buffer_bit;
        break;
    case zest_purpose_storage_buffer_write:
        usage.access_mask = zest_access_shader_write_bit;
        usage.stage_mask = relevant_pipeline_stages;
        resource->buffer_desc.buffer_info.buffer_usage_flags |= zest_buffer_usage_storage_buffer_bit;
        break;
    case zest_purpose_storage_buffer_read_write:
        usage.access_mask = zest_access_shader_read_bit | zest_access_shader_write_bit;
        usage.stage_mask = relevant_pipeline_stages;
        resource->buffer_desc.buffer_info.buffer_usage_flags |= zest_buffer_usage_storage_buffer_bit;
        break;
    case zest_purpose_transfer_buffer:
        usage.access_mask = zest_access_transfer_read_bit | zest_access_transfer_write_bit;
        usage.stage_mask = zest_pipeline_stage_transfer_bit;
        resource->buffer_desc.buffer_info.buffer_usage_flags |= zest_buffer_usage_transfer_src_bit | zest_buffer_usage_transfer_dst_bit;
        break;
    default:
        ZEST_ASSERT(0);     //Unhandled buffer access purpose! Make sure you pass in a valid zest_resource_purpose
        return;
    }

	zest_context context = zest__frame_graph_builder->context;
    if (is_output) { // Or derive is_output from purpose (e.g. WRITE implies output)
        zest_map_linear_insert(context->frame_graph_allocator[context->current_fif], pass_node->outputs, resource->name, usage);
		pass_node->output_key += resource->id + zest_Hash(&usage, sizeof(zest_resource_usage_t), 0);
    } else {
		resource->reference_count++;
        zest_map_linear_insert(context->frame_graph_allocator[context->current_fif], pass_node->inputs, resource->name, usage);
    }
}

zest_submission_batch_t *zest__get_submission_batch(zest_uint submission_id) {
    zest_uint submission_index = ZEST__SUBMISSION_INDEX(submission_id);
    zest_uint queue_index = ZEST__QUEUE_INDEX(submission_id);
    zest_frame_graph frame_graph = zest__frame_graph_builder->frame_graph;
    ZEST_ASSERT_HANDLE(frame_graph);
    return &frame_graph->submissions[submission_index].batches[queue_index];
}

void zest__set_rg_error_status(zest_frame_graph frame_graph, zest_frame_graph_result result) {
    ZEST__FLAG(frame_graph->error_status, result);
	zest_context context = zest__frame_graph_builder->context;
	ZEST__UNFLAG(context->flags, zest_context_flag_building_frame_graph);
}

zest_image_usage_flags zest__get_image_usage_from_state(zest_resource_state state) {
    switch (state) {
    case zest_resource_state_render_target:
        return zest_image_usage_color_attachment_bit;
    case zest_resource_state_depth_stencil_write:
    case zest_resource_state_depth_stencil_read:
        return zest_image_usage_depth_stencil_attachment_bit;
    case zest_resource_state_shader_read:
        // This state could be fulfilled by a sampled image or an input attachment.
        // To be safe, you can require both, or have more specific states.
        // Let's assume for now it implies sampled.
        return zest_image_usage_sampled_bit;
    case zest_resource_state_unordered_access:
        return zest_image_usage_storage_bit;
    case zest_resource_state_copy_src:
        return zest_image_usage_transfer_src_bit;
    case zest_resource_state_copy_dst:
        return zest_image_usage_transfer_dst_bit;
    case zest_resource_state_present:
        // Presenting requires being a color attachment.
        return zest_image_usage_color_attachment_bit;
    default:
        return 0;
    }
}

zest_resource_usage_t zest__configure_image_usage(zest_resource_node resource, zest_resource_purpose purpose, zest_format format, zest_load_op load_op, zest_load_op stencil_load_op, zest_pipeline_stage_flags relevant_pipeline_stages) {

    zest_resource_usage_t usage = ZEST__ZERO_INIT(zest_resource_usage_t);

    usage.resource_node = resource;
    usage.aspect_flags = zest__determine_aspect_flag(format);   // Default based on format
    usage.load_op = load_op;
    usage.stencil_load_op = stencil_load_op;

    switch (purpose) {
    case zest_purpose_color_attachment_write:
        usage.image_layout = zest_image_layout_color_attachment_optimal;
        usage.access_mask = zest_access_color_attachment_write_bit;
        // If load_op is zest_attachment_load_op_load, AN IMPLICIT READ OCCURS.
        // If blending with destination, zest_access_color_attachment_read_bit IS ALSO INVOLVED.
        // For simplicity, if this is purely about the *write* aspect:
        if (load_op == zest_load_op_load) {
            usage.access_mask |= zest_access_color_attachment_read_bit; // fOR THE LOAD ITSElf
        }
        usage.stage_mask = zest_pipeline_stage_color_attachment_output_bit;
        usage.aspect_flags = zest_image_aspect_color_bit;
        usage.is_output = ZEST_TRUE;
        resource->image.info.flags |= zest_image_flag_color_attachment;
        break;

    case zest_purpose_color_attachment_read:
        usage.image_layout = zest_image_layout_color_attachment_optimal;
        usage.access_mask = zest_access_color_attachment_read_bit;
        usage.stage_mask = zest_pipeline_stage_color_attachment_output_bit;
        usage.aspect_flags = zest_image_aspect_color_bit;
        resource->image.info.flags |= zest_image_flag_color_attachment;
        usage.is_output = ZEST_FALSE;
        break;

    case zest_purpose_sampled_image:
        usage.image_layout = zest_image_layout_shader_read_only_optimal;
        usage.access_mask = zest_access_shader_read_bit;
        usage.stage_mask = relevant_pipeline_stages;
        resource->image.info.flags |= zest_image_flag_sampled;
        usage.is_output = ZEST_FALSE;
        break;

    case zest_purpose_storage_image_read:
        usage.image_layout = zest_image_layout_general;
        usage.access_mask = zest_access_shader_read_bit;
        usage.stage_mask = relevant_pipeline_stages;
        resource->image.info.flags |= zest_image_flag_sampled | zest_image_flag_storage;
        usage.is_output = ZEST_FALSE;
        break;

    case zest_purpose_storage_image_write:
        usage.image_layout = zest_image_layout_general;
        usage.access_mask = zest_access_shader_write_bit;
        usage.stage_mask = relevant_pipeline_stages;
        resource->image.info.flags |= zest_image_flag_storage;
        usage.is_output = ZEST_TRUE;
        break;

    case zest_purpose_storage_image_read_write:
        usage.image_layout = zest_image_layout_general;
        usage.access_mask = zest_access_shader_read_bit | zest_access_shader_write_bit;
        usage.stage_mask = relevant_pipeline_stages;
        resource->image.info.flags |= zest_image_flag_sampled | zest_image_flag_storage;
        usage.is_output = ZEST_TRUE;
        break;

    case zest_purpose_depth_stencil_attachment_read:
        usage.image_layout = zest_image_layout_depth_stencil_read_only_optimal;
        usage.access_mask = zest_access_depth_stencil_attachment_read_bit;
        usage.stage_mask = zest_pipeline_stage_early_fragment_tests_bit | zest_pipeline_stage_late_fragment_tests_bit;
        usage.aspect_flags = zest_image_aspect_depth_bit;
        resource->image.info.flags |= zest_image_flag_depth_stencil_attachment;
        usage.is_output = ZEST_FALSE;
        break;

    case zest_purpose_depth_stencil_attachment_write:
        usage.image_layout = zest_image_layout_depth_stencil_attachment_optimal;
        usage.access_mask = zest_access_depth_stencil_attachment_write_bit;
        if (load_op == zest_load_op_load || stencil_load_op == zest_load_op_load) {
            usage.access_mask |= zest_access_depth_stencil_attachment_read_bit;
        }
        usage.aspect_flags = zest_image_aspect_depth_bit;
        usage.stage_mask = zest_pipeline_stage_early_fragment_tests_bit | zest_pipeline_stage_late_fragment_tests_bit;
        resource->image.info.flags |= zest_image_flag_depth_stencil_attachment;
        usage.is_output = ZEST_TRUE;
        break;

    case zest_purpose_depth_stencil_attachment_read_write: // Typical depth testing and writing
        usage.image_layout = zest_image_layout_depth_stencil_attachment_optimal;
        usage.access_mask = zest_access_depth_stencil_attachment_read_bit | zest_access_depth_stencil_attachment_write_bit;
        usage.stage_mask = zest_pipeline_stage_early_fragment_tests_bit | zest_pipeline_stage_late_fragment_tests_bit;
        usage.aspect_flags = zest_image_aspect_depth_bit;
        resource->image.info.flags |= zest_image_flag_depth_stencil_attachment;
        usage.is_output = ZEST_TRUE; // Modifies resource
        break;

    case zest_purpose_input_attachment: // For subpass reads
        usage.image_layout = zest_image_layout_shader_read_only_optimal;
        usage.access_mask = zest_access_input_attachment_read_bit;
        usage.stage_mask = relevant_pipeline_stages;
        resource->image.info.flags |= zest_image_flag_input_attachment;
        usage.is_output = ZEST_FALSE;
        break;

    case zest_purpose_transfer_image:
        usage.image_layout = zest_image_layout_transfer_src_optimal;
        usage.access_mask = zest_access_transfer_read_bit;
        usage.stage_mask = zest_pipeline_stage_transfer_bit;
        resource->image.info.flags |= zest_image_flag_transfer_src | zest_image_flag_transfer_dst;
        usage.is_output = ZEST_FALSE;
        break;

    case zest_purpose_present_src:
        usage.image_layout = zest_image_layout_present;
        usage.access_mask = 0; // No specific GPU access by the pass itself for this state.
        usage.stage_mask = zest_pipeline_stage_bottom_of_pipe_bit; // ENSURE ALL PRIOR WORK IS DONE.
        resource->image.info.flags |= zest_image_flag_color_attachment;
        usage.is_output = ZEST_TRUE;
        break;

    default:
        ZEST_ASSERT(0); //Unhandled image access purpose!"
    }
	return usage;
}

void zest__add_pass_image_usage(zest_pass_node pass_node, zest_resource_node image_resource, zest_resource_purpose purpose, zest_pipeline_stage_flags relevant_pipeline_stages, zest_bool is_output, zest_load_op load_op, zest_store_op store_op, zest_load_op stencil_load_op, zest_store_op stencil_store_op, zest_clear_value_t clear_value) {
    zest_resource_usage_t usage = zest__configure_image_usage(image_resource, purpose, image_resource->image.info.format, load_op, stencil_load_op, relevant_pipeline_stages);

    // Attachment ops are only relevant for attachment purposes
    // For other usages, they'll default to 0/DONT_CARE which is fine.
    usage.store_op = store_op;
    usage.stencil_store_op = stencil_store_op;
    usage.clear_value = clear_value;
    usage.purpose = purpose;
	zest_context context = zest__frame_graph_builder->context;
  
    if (usage.is_output) {
		//This is the first time this resource has been used as output
		usage.resource_node = image_resource;
		zest_map_linear_insert(context->frame_graph_allocator[context->current_fif], pass_node->outputs, image_resource->name, usage);
		pass_node->output_key += image_resource->id + zest_Hash(&usage, sizeof(zest_resource_usage_t), 0);
    } else {
        usage.resource_node = image_resource;
        ZEST_ASSERT(usage.resource_node);
		image_resource->reference_count++;
		zest_map_linear_insert(context->frame_graph_allocator[context->current_fif], pass_node->inputs, image_resource->name, usage);
    }
}

void zest_ReleaseBufferAfterUse(zest_resource_node node) {
    ZEST_ASSERT(ZEST__FLAGGED(node->flags, zest_resource_node_flag_imported));  //The resource must be imported, transient buffers are simply discarded after use.
    ZEST__FLAG(node->flags, zest_resource_node_flag_release_after_use);
}

// --- Image Helpers ---
void zest_ConnectInput(zest_resource_node resource) {
    ZEST_ASSERT_HANDLE(zest__frame_graph_builder->frame_graph);  //This function must be called withing a Being/EndRenderGraph block
	zest_context context = zest__frame_graph_builder->context;
    ZEST_ASSERT_HANDLE(zest__frame_graph_builder->current_pass);          //No current pass found. Make sure you call zest_BeginPass
    zest_pass_node pass = zest__frame_graph_builder->current_pass;
    if(!ZEST_VALID_HANDLE(resource)) return;  
    zest_supported_pipeline_stages stages = zest_pipeline_no_stage;
    zest_resource_purpose purpose = zest_purpose_none;
    if (resource->type & zest_resource_type_is_image) {
        switch (pass->type) {
        case zest_pass_type_graphics:
            stages = zest_pipeline_fragment_stage;
            purpose = zest_purpose_sampled_image;
            break;
        case zest_pass_type_compute:
            stages = zest_pipeline_compute_stage;
            purpose = zest_purpose_storage_image_read;
            break;
        case zest_pass_type_transfer:
            stages = zest_pipeline_transfer_stage;
            purpose = zest_purpose_transfer_image;
            break;
        }
        zest__add_pass_image_usage(pass, resource, purpose, stages, ZEST_FALSE,
            zest_load_op_dont_care, zest_store_op_dont_care, zest_load_op_dont_care, zest_store_op_dont_care, ZEST__ZERO_INIT(zest_clear_value_t));
    } else {
        //Buffer input
        switch (pass->type) {
        case zest_pass_type_graphics: {
            stages = zest_pipeline_vertex_input_stage;
			if (resource->type & zest_resource_type_index_buffer) {
				purpose = zest_purpose_index_buffer;
			} else {
				purpose = zest_purpose_vertex_buffer;
			}
            break;
        }
        case zest_pass_type_compute: {
            stages = zest_pipeline_compute_stage;
            purpose = zest_purpose_storage_buffer_read_write;
            break;
        }
        case zest_pass_type_transfer: {
            stages = zest_pipeline_transfer_stage;
            purpose = zest_purpose_transfer_buffer;
            break;
        }
        }
		zest__add_pass_buffer_usage(pass, resource, purpose, stages, ZEST_FALSE);
    }
}

void zest_ConnectSwapChainOutput() {
    ZEST_ASSERT_HANDLE(zest__frame_graph_builder->frame_graph);  //This function must be called withing a Being/EndRenderGraph block
	zest_context context = zest__frame_graph_builder->context;
    ZEST_ASSERT_HANDLE(zest__frame_graph_builder->current_pass); //No current pass found. Make sure you call zest_BeginPass
    zest_pass_node pass = zest__frame_graph_builder->current_pass;
    zest_frame_graph frame_graph = zest__frame_graph_builder->frame_graph;
    ZEST_ASSERT_HANDLE(frame_graph->swapchain_resource);  //Not a valid swapchain resource, did you call zest_ImportSwapchainResource?
    zest_clear_value_t cv = frame_graph->swapchain->clear_color;
    // Assuming clear for swapchain if not explicitly loaded
    ZEST__FLAG(pass->flags, zest_pass_flag_do_not_cull);
    zest__add_pass_image_usage(pass, frame_graph->swapchain_resource, zest_purpose_color_attachment_write, 
        zest_pipeline_stage_color_attachment_output_bit, ZEST_TRUE,
        zest_load_op_clear, zest_store_op_store, zest_load_op_dont_care, zest_store_op_dont_care, cv);
}

void zest_ConnectOutput(zest_resource_node resource) {
    ZEST_ASSERT_HANDLE(zest__frame_graph_builder->frame_graph);  //This function must be called withing a Being/EndRenderGraph block
	zest_context context = zest__frame_graph_builder->context;
    ZEST_ASSERT_HANDLE(zest__frame_graph_builder->current_pass); //No current pass found. Make sure you call zest_BeginPass
    zest_pass_node pass = zest__frame_graph_builder->current_pass;
    if (!ZEST_VALID_HANDLE(resource)) return;
    if (resource->image.info.sample_count > 1) {
        ZEST__FLAG(pass->flags, zest_pass_flag_output_resolve);
    }
    if (resource->type & zest_resource_type_is_image) {
        zest_clear_value_t cv = ZEST__ZERO_INIT(zest_clear_value_t);
        switch (pass->type) {
        case zest_pass_type_graphics: {
            ZEST_ASSERT(resource->type & zest_resource_type_is_image); //Resource must be an image buffer when used as output in a graphics pass
            if (resource->image.info.format == context->device->depth_format) {
                cv.depth_stencil.depth = 1.f;
                cv.depth_stencil.stencil = 0;
                zest__add_pass_image_usage(pass, resource, zest_purpose_depth_stencil_attachment_write,
                    0, ZEST_TRUE,
                    zest_load_op_clear, zest_store_op_dont_care,
                    zest_load_op_dont_care, zest_store_op_dont_care,
                    cv);
            } else {
                cv.color = resource->clear_color;
                zest__add_pass_image_usage(pass, resource, zest_purpose_color_attachment_write,
                    zest_pipeline_stage_color_attachment_output_bit, ZEST_TRUE,
                    zest_load_op_clear, zest_store_op_store,
                    zest_load_op_dont_care, zest_store_op_dont_care,
                    cv);
            }
            break;
        }
        case zest_pass_type_compute: {
            zest__add_pass_image_usage(pass, resource, zest_purpose_storage_image_write, zest_pipeline_compute_stage,
                ZEST_FALSE, zest_load_op_dont_care, zest_store_op_dont_care,
                zest_load_op_dont_care, zest_store_op_dont_care, ZEST__ZERO_INIT(zest_clear_value_t));
            break;
        }
        case zest_pass_type_transfer: {
            zest__add_pass_image_usage(pass, resource, zest_purpose_transfer_image, zest_pipeline_compute_stage,
                ZEST_FALSE, zest_load_op_dont_care, zest_store_op_dont_care,
                zest_load_op_dont_care, zest_store_op_dont_care, ZEST__ZERO_INIT(zest_clear_value_t));
            break;
        }
        }
    } else {
        //Buffer output
        switch (pass->type) {
        case zest_pass_type_graphics: {
			if (resource->type & zest_resource_type_is_image) {
				zest__add_pass_buffer_usage(pass, resource, zest_purpose_storage_image_write,
											zest_pipeline_stage_color_attachment_output_bit, ZEST_TRUE);
			} else {
				zest__add_pass_buffer_usage(pass, resource, zest_purpose_transfer_buffer,
											zest_pipeline_transfer_stage, ZEST_TRUE);
			}
            break;

        }
        case zest_pass_type_compute: {
            zest__add_pass_buffer_usage(pass, resource, zest_purpose_storage_buffer_write,
                zest_pipeline_compute_stage, ZEST_TRUE);
            break;
        }
        case zest_pass_type_transfer: {
            zest__add_pass_buffer_usage(pass, resource, zest_purpose_transfer_buffer, 
                zest_pipeline_transfer_stage, ZEST_TRUE);
            break;
        }
        }

    }
}

void zest_ConnectGroupedOutput(zest_output_group group) {
    ZEST_ASSERT_HANDLE(zest__frame_graph_builder->frame_graph);  //This function must be called withing a Being/EndRenderGraph block
	zest_context context = zest__frame_graph_builder->context;
    ZEST_ASSERT_HANDLE(zest__frame_graph_builder->current_pass);          //No current pass found. Make sure you call zest_BeginPass
    zest_pass_node pass = zest__frame_graph_builder->current_pass;
    zest_frame_graph frame_graph = zest__frame_graph_builder->frame_graph;
    ZEST_ASSERT_HANDLE(group);  //Not a valid render target group
    ZEST_ASSERT_HANDLE(pass);  //Not a valid pass node
    ZEST_ASSERT(zest_vec_size(group->resources));   //There are no resources in the group!
    zest_vec_foreach(i, group->resources) {
        zest_resource_node resource = group->resources[i];
        switch (resource->type) {
			case zest_resource_type_swap_chain_image: zest_ConnectSwapChainOutput(); break;
			default: zest_ConnectOutput(resource); break;
        }
    }
}

void zest_WaitOnTimeline(zest_execution_timeline timeline) {
    ZEST_ASSERT_HANDLE(timeline);    //Not a valid execution timeline. Use zest_CreateExecutionTimeline to create one
    ZEST_ASSERT_HANDLE(zest__frame_graph_builder->frame_graph);  //This function must be called withing a Being/EndRenderGraph block
    zest_frame_graph frame_graph = zest__frame_graph_builder->frame_graph;
	zest_context context = zest__frame_graph_builder->context;
    zest_vec_linear_push(context->frame_graph_allocator[context->current_fif], frame_graph->wait_on_timelines, timeline);
}

void zest_SignalTimeline(zest_execution_timeline timeline) {
    ZEST_ASSERT_HANDLE(timeline);    //Not a valid execution timeline. Use zest_CreateExecutionTimeline to create one
    ZEST_ASSERT_HANDLE(zest__frame_graph_builder->frame_graph);  //This function must be called withing a Being/EndRenderGraph block
    zest_frame_graph frame_graph = zest__frame_graph_builder->frame_graph;
	zest_context context = zest__frame_graph_builder->context;
    zest_vec_linear_push(context->frame_graph_allocator[context->current_fif], frame_graph->signal_timelines, timeline);
}

zest_execution_timeline zest_CreateExecutionTimeline(zest_context context) {
    zest_execution_timeline timeline = (zest_execution_timeline)ZEST__NEW(context->device->allocator, zest_execution_timeline);
    *timeline = ZEST__ZERO_INIT(zest_execution_timeline_t);
    timeline->magic = zest_INIT_MAGIC(zest_struct_type_execution_timeline);
    timeline->current_value = 0;
    if (!context->device->platform->create_execution_timeline_backend(context, timeline)) {
        if (timeline->backend) {
            ZEST__FREE(context->device->allocator, timeline->backend);
        }
		ZEST__FREE(context->device->allocator, timeline);
		return NULL;
    }
    zest_vec_push(context->device->allocator, context->device->timeline_semaphores, timeline);
    return timeline;
}

// --End frame graph functions

// [Swapchain]
zest_swapchain zest_GetSwapchain(zest_context context) {
    return context->swapchain;
}

zest_format zest_GetSwapchainFormat(zest_swapchain swapchain) {
    ZEST_ASSERT_HANDLE(swapchain);  //Not a valid swapchain handle
    return swapchain->format;
}

void zest_SetSwapchainClearColor(zest_context context, float red, float green, float blue, float alpha) {
    context->swapchain->clear_color = ZEST_STRUCT_LITERAL(zest_clear_value_t, red, green, blue, alpha);
}

zest_layer_handle zest__create_instance_layer(zest_context context, const char *name, zest_size instance_type_size, zest_uint initial_instance_count) {
    zest_layer layer;
    zest_layer_handle handle = zest__new_layer(context, &layer);
    layer->name = name;
    zest__initialise_instance_layer(context, layer, instance_type_size, initial_instance_count);
    return handle;
}

zest_layer_handle zest_CreateMeshLayer(zest_context context, const char* name, zest_size vertex_type_size) {
    zest_layer layer;
    zest_layer_handle handle = zest__new_layer(context, &layer);
    layer->name = name;
    zest__initialise_mesh_layer(context, layer, sizeof(zest_textured_vertex_t), 1000);
    return handle;
}

zest_layer_handle zest_CreateInstanceMeshLayer(zest_context context, const char* name) {
    zest_layer layer;
    zest_layer_handle handle = zest__new_layer(context, &layer);
    layer->name = name;
    zest__initialise_instance_layer(context, layer, sizeof(zest_mesh_instance_t), 1000);
    return handle;
}

// --Texture and Image functions
zest_image_handle zest__new_image(zest_context context) {
    zest_image_handle handle = { zest__add_store_resource(zest_handle_type_images, context) };
    zest_image image = (zest_image)zest__get_store_resource_checked(context, handle.value);
    *image = ZEST__ZERO_INIT(zest_image_t);
    image->magic = zest_INIT_MAGIC(zest_struct_type_image);
    image->backend = (zest_image_backend)context->device->platform->new_image_backend(context);
    for (int i = 0; i != zest_max_global_binding_number; ++i) {
        image->bindless_index[i] = ZEST_INVALID;
    }
	handle.context = context;
	image->handle = handle;
    return handle;
}

zest_bool zest__transition_image_layout(zest_image image, zest_image_layout new_layout, zest_uint base_mip_index, zest_uint mip_levels, zest_uint base_array_index, zest_uint layer_count) {
	mip_levels = ZEST__MIN(mip_levels, image->info.mip_levels);
	layer_count = ZEST__MIN(layer_count, image->info.layer_count);
	zest_context context = image->handle.context;
	if (context->device->platform->transition_image_layout(image, new_layout, base_mip_index, mip_levels, base_array_index, layer_count)) {
		image->info.layout = new_layout;
		return ZEST_TRUE;
	}
	return ZEST_FALSE;
}

zest_byte zest__calculate_texture_layers(zloc_linear_allocator_t *allocator, zestrp_rect* rects, zest_uint width, zest_uint height, const zest_uint node_count) {
	zest_size marker = zloc_GetMarker(allocator);
    zestrp_node* nodes = 0;
    zest_vec_linear_resize(allocator, nodes, node_count);
    zest_byte layers = 0;
    zestrp_rect* current_rects = 0;
    zestrp_rect* rects_copy = 0;
    zest_vec_linear_resize(allocator, rects_copy, zest_vec_size(rects));
    memcpy(rects_copy, rects, zest_vec_size_in_bytes(rects));
    while (!zest_vec_empty(rects_copy) && layers <= 255) {

        zestrp_context context;
        zestrp_init_target(&context, width, height, nodes, node_count);
        zestrp_pack_rects(&context, rects_copy, (int)zest_vec_size(rects_copy));

        zest_vec_foreach(i, rects_copy) {
            zest_vec_linear_push(allocator, current_rects, rects_copy[i]);
        }

        zest_vec_clear(rects_copy);

        zest_vec_foreach(i, current_rects) {
            if (!rects_copy[i].was_packed) {
                zest_vec_linear_push(allocator, rects_copy, rects_copy[i]);
            }
        }

        zest_vec_clear(current_rects);
        layers++;
    }

	zloc_ResetToMarker(allocator, marker);
    return layers;
}

zest_bool zest_GetBestFit(zest_image_collection_handle image_collection_handle, zest_uint *width, zest_uint *height) {
    zestrp_rect* rects = 0;
    zestrp_rect* rects_to_process = 0;

	zest_image_collection image_collection = (zest_image_collection)zest__get_store_resource(image_collection_handle.context, image_collection_handle.value);
	zest_context context = image_collection->handle.context;
	zloc_linear_allocator_t *allocator = context->device->scratch_arena;

	zest_uint max_image_size = zest_GetMaxImageSize(context);

	ZEST_ASSERT(max_image_size > *width, "Width you passed in is greater then the maximum available image size");
	ZEST_ASSERT(max_image_size > *height, "Height you passed in is greater then the maximum available image size");

    zest_map_foreach(i, image_collection->image_bitmaps) {
        zest_bitmap bitmap = image_collection->image_bitmaps.data[i];
        zestrp_rect rect;

        rect.w = bitmap->meta.width + image_collection->packed_border_size * 2;
        rect.h = bitmap->meta.height + image_collection->packed_border_size * 2;
        rect.x = 0;
        rect.y = 0;
        rect.was_packed = 0;
        rect.id = i;

        zest_vec_linear_push(allocator, rects, rect);
    }

	zest_uint dim_flip = 1;
	zest_size marker = zloc_GetMarker(allocator);

	while (1) {
		const zest_uint node_count = *width;
		zestrp_node* nodes = 0;
		zest_vec_linear_resize(allocator, nodes, node_count);

		zestrp_context rp_context;
		zestrp_init_target(&rp_context, *width, *height, nodes, node_count);
		if (zestrp_pack_rects(&rp_context, rects, (int)zest_vec_size(rects))) {
			*width = 0;
			*height = 0;
			zest_vec_foreach(i, rects) {
				*width = ZEST__MAX(*width, (zest_uint)rects[i].x + rects[i].w);
				*height = ZEST__MAX(*height, (zest_uint)rects[i].y + rects[i].h);
			}
			*width = (zest_uint)zest_GetNextPower(*width);
			*height = (zest_uint)zest_GetNextPower(*height);
			break;
		} else {
			if (dim_flip) {
				*width *= 2;
			} else {
				*height *= 2;
			}
			dim_flip = dim_flip ^ 1;
		}
		zloc_ResetToMarker(allocator, marker);
	}
	zloc_ResetLinearAllocator(allocator);
	return ZEST_TRUE;
}

void zest__pack_images(zest_image_collection atlas, zest_uint layer_width, zest_uint layer_height) {
    zestrp_rect* rects = 0;
    zestrp_rect* rects_to_process = 0;

	zest_context context = atlas->handle.context;
	zloc_linear_allocator_t *allocator = context->device->scratch_arena;

    zest_map_foreach(i, atlas->image_bitmaps) {
        zest_bitmap bitmap = atlas->image_bitmaps.data[i];
        zestrp_rect rect;

        rect.w = bitmap->meta.width + atlas->packed_border_size * 2;
        rect.h = bitmap->meta.height + atlas->packed_border_size * 2;
        rect.x = 0;
        rect.y = 0;
        rect.was_packed = 0;
        rect.id = i;

        ZEST_ASSERT(layer_width > (zest_uint)rect.w, "An bitmap in the collection is too big for the layer size in the image");		
        ZEST_ASSERT(layer_height > (zest_uint)rect.h, "An bitmap in the collection is too big for the layer size in the image");

        zest_vec_linear_push(allocator, rects, rect);
        zest_vec_linear_push(allocator, rects_to_process, rect);
    }

    const zest_uint node_count = layer_width;
    zestrp_node* nodes = 0;
    zest_vec_linear_resize(allocator, nodes, node_count);

	atlas->layer_count = zest__calculate_texture_layers(allocator, rects, layer_width, layer_height, node_count);

	zest_FreeBitmapArray(&atlas->bitmap_array);
	zest_format format = atlas->format;
	zest_CreateBitmapArray(context, &atlas->bitmap_array, layer_width, layer_height, format, atlas->layer_count);

    zest_byte current_layer = 0;

    zest_vec_foreach(i, atlas->layers) {
        zest_FreeBitmapData(&atlas->layers[i]);
    }
    zest_vec_clear(atlas->layers);

    zest_color_t fillcolor;
    fillcolor.r = 0;
    fillcolor.g = 0;
    fillcolor.b = 0;
    fillcolor.a = 0;

    zestrp_rect* current_rects = 0;
    while (!zest_vec_empty(rects) && current_layer < atlas->layer_count) {
        zestrp_context rp_context;
        zestrp_init_target(&rp_context, layer_width, layer_height, nodes, node_count);
        zestrp_pack_rects(&rp_context, rects, (int)zest_vec_size(rects));

        zest_vec_foreach(i, rects) {
            zest_vec_linear_push(allocator, current_rects, rects[i]);
        }

        zest_vec_clear(rects);

        zest_bitmap_t tmp_image = { 0 };
        tmp_image.magic = zest_INIT_MAGIC(zest_struct_type_bitmap);
		tmp_image.context = context;
        zest_AllocateBitmap(&tmp_image, layer_width, layer_height, format);
        int count = 0;

        zest_vec_foreach(i, current_rects) {
            zestrp_rect* rect = &current_rects[i];

            if (rect->was_packed) {
                zest_atlas_region region = atlas->image_bitmaps.data[rect->id]->atlas_region;

                float rect_x = (float)rect->x + atlas->packed_border_size;
                float rect_y = (float)rect->y + atlas->packed_border_size;

                region->uv.x = (rect_x + 0.5f) / (float)layer_width;
                region->uv.y = (rect_y + 0.5f) / (float)layer_height;
                region->uv.z = ((float)region->width + (rect_x - 0.5f)) / (float)layer_width;
                region->uv.w = ((float)region->height + (rect_y - 0.5f)) / (float)layer_height;
                region->uv_packed = zest_Pack16bit4SNorm(region->uv.x, region->uv.y, region->uv.z, region->uv.w);
                region->left = (zest_uint)rect_x;
                region->top = (zest_uint)rect_y;
                region->layer_index = current_layer;

                zest_CopyBitmap(atlas->image_bitmaps.data[rect->id], 0, 0, region->width, region->height, &tmp_image, region->left, region->top);

                count++;
            }
            else {
                zest_vec_linear_push(allocator, rects, *rect);
            }
        }

        zloc_SafeCopyBlock(atlas->bitmap_array.data, zest_BitmapArrayLookUp(&atlas->bitmap_array, current_layer), tmp_image.data, atlas->bitmap_array.meta[current_layer].size);

        zest_vec_push(context->device->allocator, atlas->layers, tmp_image);
        current_layer++;
        zest_vec_clear(current_rects);
    }

    size_t offset = 0;

    zest_vec_clear(atlas->buffer_copy_regions);

	zest_bitmap_array_t *bitmap_array = &atlas->bitmap_array;
    zest_vec_foreach(i, bitmap_array->meta) {
        zest_buffer_image_copy_t buffer_copy_region = ZEST__ZERO_INIT(zest_buffer_image_copy_t);
        buffer_copy_region.image_aspect = zest_image_aspect_color_bit;
        buffer_copy_region.mip_level = 0;
        buffer_copy_region.base_array_layer = i;
        buffer_copy_region.layer_count = 1;
        buffer_copy_region.image_extent.width = bitmap_array->meta[i].width;
        buffer_copy_region.image_extent.height = bitmap_array->meta[i].height;
        buffer_copy_region.image_extent.depth = 1;
        buffer_copy_region.buffer_offset = bitmap_array->meta[i].offset;

        zest_vec_push(context->device->allocator, atlas->buffer_copy_regions, buffer_copy_region);
    }

	zloc_ResetLinearAllocator(allocator);
}

void zest__release_all_global_texture_indexes(zest_image image) {
	zest_context context = image->handle.context;
    if (!context->device->global_bindless_set_layout_handle.value) {
        return;
    }
    for (int i = 0; i != zest_max_global_binding_number; ++i) {
        if (image->bindless_index[i] != ZEST_INVALID) {
            zest__release_bindless_index(context->device->global_bindless_set_layout, (zest_binding_number_type)i, image->bindless_index[i]);
            image->bindless_index[i] = ZEST_INVALID;
        }
    }
    zest_map_foreach(i, image->mip_indexes) {
        zest_mip_index_collection *mip_collection = &image->mip_indexes.data[i];
        zest_vec_foreach(j, mip_collection->mip_indexes) {
            zest_uint index = mip_collection->mip_indexes[j];
            zest_ReleaseGlobalBindlessIndex(context, index, mip_collection->binding_number);
        }
    }
}

zest_imgui_image_t zest_NewImGuiImage(void) {
    zest_imgui_image_t imgui_image = ZEST__ZERO_INIT(zest_imgui_image_t);
    imgui_image.magic = zest_INIT_MAGIC(zest_struct_type_imgui_image);
    return imgui_image;
}

zest_atlas_region zest_CreateAtlasRegion(zest_context context) {
    zest_atlas_region region = (zest_atlas_region)ZEST__NEW_ALIGNED(context->device->allocator, zest_atlas_region, 16);
    *region = ZEST__ZERO_INIT(zest_atlas_region_t);
    region->magic = zest_INIT_MAGIC(zest_struct_type_atlas_region);
	region->context = context;
    region->uv.x = 0.f;
    region->uv.y = 0.f;
    region->uv.z = 1.f;
    region->uv.w = 1.f;
    region->layer_index = 0;
    region->frames = 1;
    return region;
}

zest_atlas_region zest_NewAtlasRegion(zest_context context) {
    zest_atlas_region region = (zest_atlas_region)ZEST__NEW_ALIGNED(context->device->allocator, zest_atlas_region, 16);
    *region = ZEST__ZERO_INIT(zest_atlas_region_t);
    region->magic = zest_INIT_MAGIC(zest_struct_type_atlas_region);
	region->context = context;
    region->uv.x = 0.f;
    region->uv.y = 0.f;
    region->uv.z = 1.f;
    region->uv.w = 1.f;
    region->layer_index = 0;
    region->frames = 1;
    return region;
}

void zest_BindAtlasRegionToImage(zest_atlas_region region, zest_uint sampler_index, zest_image_handle image_handle, zest_binding_number_type binding_number) {
	ZEST_ASSERT_HANDLE(region);		//Not a valid region pointer
	zest_image image = (zest_image)zest__get_store_resource_checked(image_handle.context, image_handle.value);
	region->image_index = image->bindless_index[binding_number];
	region->sampler_index = sampler_index;
	region->binding_number = binding_number;
}

void zest_FreeAtlasRegion(zest_atlas_region region) {
    if (ZEST_VALID_HANDLE(region)) {
        ZEST__FREE(region->context->device->allocator, region);
    }
}

zest_atlas_region zest_CreateAnimation(zest_context context, zest_uint frames) {
    zest_atlas_region image = (zest_atlas_region)ZEST__ALLOCATE_ALIGNED(context->device->allocator, sizeof(zest_atlas_region_t) * frames, 16);
    ZEST_ASSERT(image); //Couldn't allocate the image. Out of memory?
	image->magic = zest_INIT_MAGIC(zest_struct_type_atlas_region);
	image->context = context;
    image->frames = frames;
    return image;
}

void zest_AllocateBitmapMemory(zest_bitmap bitmap, zest_size size_in_bytes) {
	ZEST_ASSERT_HANDLE(bitmap);	//Not a valid bitmap handle
    bitmap->meta.size = size_in_bytes;
	bitmap->data = (zest_byte*)ZEST__ALLOCATE(bitmap->context->device->allocator, size_in_bytes);
}

zest_bool zest_AllocateBitmap(zest_bitmap bitmap, int width, int height, zest_format format) {
	ZEST_ASSERT_HANDLE(bitmap);	//Not a valid bitmap handle
	zest__get_format_pixel_data(format, &bitmap->meta.channels, &bitmap->meta.bytes_per_pixel);
    bitmap->meta.size = width * height * bitmap->meta.channels * bitmap->meta.bytes_per_pixel;
    if (bitmap->meta.size > 0) {
        bitmap->data = (zest_byte*)ZEST__ALLOCATE(bitmap->context->device->allocator, bitmap->meta.size);
        bitmap->meta.width = width;
        bitmap->meta.height = height;
        bitmap->meta.format = format;
        bitmap->meta.stride = width * bitmap->meta.bytes_per_pixel;
        memset(bitmap->data, 0, bitmap->meta.size);
		return ZEST_TRUE;
    }
	ZEST_PRINT("Format not supported for bitmaps");
	return ZEST_FALSE;
}

void zest_FreeBitmap(zest_bitmap image) {
	ZEST_ASSERT_HANDLE(image);		//Not a valid bitmap handle
    if (!image->is_imported && image->data) {
        ZEST__FREE(image->context->device->allocator, image->data);
    }
    zest_FreeText(image->context->device->allocator, &image->name);
    image->data = ZEST_NULL;
    ZEST__FREE(image->context->device->allocator, image);
}

void zest_FreeBitmapData(zest_bitmap image) {
    if (!image->is_imported && image->data) {
        ZEST__FREE(image->context->device->allocator, image->data);
    }
    zest_FreeText(image->context->device->allocator, &image->name);
    image->data = ZEST_NULL;
}

zest_bitmap zest_NewBitmap(zest_context context) {
    zest_bitmap bitmap = (zest_bitmap)ZEST__NEW(context->device->allocator, zest_bitmap);
    *bitmap = ZEST__ZERO_INIT(zest_bitmap_t);
    bitmap->magic = zest_INIT_MAGIC(zest_struct_type_bitmap);
	bitmap->context = context;
    return bitmap;
}

zest_bitmap zest_CreateBitmapFromRawBuffer(zest_context context, const char* name, void* pixels, int size, int width, int height, zest_format format) {
    zest_bitmap bitmap = zest_NewBitmap(context);
    bitmap->is_imported = 1;
	zest__get_format_pixel_data(format, &bitmap->meta.channels, &bitmap->meta.bytes_per_pixel);
    bitmap->data = (zest_byte*)pixels;
    bitmap->meta.width = width;
    bitmap->meta.height = height;
    bitmap->meta.size = size;
    bitmap->meta.stride = width * bitmap->meta.bytes_per_pixel;
    bitmap->meta.format = format;
	zest_size expected_size = height * bitmap->meta.stride;
	ZEST_ASSERT(expected_size == size, "The size of the allocated bitmap is not the same size as the raw pixel data, check to make sure you're passing in the correct format.");
    if (name) {
        zest_SetText(context->device->allocator, &bitmap->name, name);
    }
    return bitmap;
}

void zest_ConvertBitmapToAlpha(zest_bitmap  image) {

    zest_size pos = 0;

    if (image->meta.channels == 4) {
        while (pos < image->meta.size) {
            zest_byte c = (zest_byte)ZEST__MIN(((float)*(image->data + pos) * 0.3f) + ((float)*(image->data + pos + 1) * .59f) + ((float)*(image->data + pos + 2) * .11f), (float)*(image->data + pos + 3));
            *(image->data + pos) = 255;
            *(image->data + pos + 1) = 255;
            *(image->data + pos + 2) = 255;
            *(image->data + pos + 3) = c;
            pos += image->meta.channels;
        }
    }
    else if (image->meta.channels == 3) {
        while (pos < image->meta.size) {
            zest_byte c = (zest_byte)(((float)*(image->data + pos) * 0.3f) + ((float)*(image->data + pos + 1) * .59f) + ((float)*(image->data + pos + 2) * .11f));
            *(image->data + pos) = c;
            *(image->data + pos + 1) = c;
            *(image->data + pos + 2) = c;
            pos += image->meta.channels;
        }
    }
    else if (image->meta.channels == 2) {
        while (pos < image->meta.size) {
            *(image->data + pos) = 255;
            pos += image->meta.channels;
        }
    }
    else if (image->meta.channels == 1) {
        return;
    }
}

void zest_ConvertBitmap(zest_bitmap src, zest_format new_format, zest_byte alpha_level) {
	ZEST_ASSERT_HANDLE(src);	//Not a valid bitmap handle
	if (src->meta.format == new_format) {
		return;
	}

    ZEST_ASSERT(src->data);	//no valid bitmap data found

	int to_channels;
	int bytes_per_pixel;
	zest__get_format_pixel_data(new_format, &to_channels, &bytes_per_pixel);
	ZEST_ASSERT(to_channels);	//Not a valid format, must be an 8bit unorm format with 1 to 4 channels.
    int from_channels = src->meta.channels;

    zest_size new_size = src->meta.width * src->meta.height * to_channels * bytes_per_pixel;
    zest_byte* new_image = (zest_byte*)ZEST__ALLOCATE(src->context->device->allocator, new_size);

    zest_byte *from_data = src->data;
    zest_byte *to_data = new_image;

    for (int y = 0; y < src->meta.height; ++y) {
        for (int x = 0; x < src->meta.width; ++x) {
            zest_color_t source_pixel = { 0, 0, 0, alpha_level };
            zest_size from_idx = (y * src->meta.width + x) * from_channels;
            zest_size to_idx = (y * src->meta.width + x) * to_channels;

            switch (from_channels) {
                case 1:
                    source_pixel.r = from_data[from_idx];
                    source_pixel.g = from_data[from_idx];
                    source_pixel.b = from_data[from_idx];
                    break;
                case 2:
                    source_pixel.r = from_data[from_idx];
                    source_pixel.g = from_data[from_idx];
                    source_pixel.b = from_data[from_idx];
					source_pixel.a = from_data[from_idx + 1];
                    break;
                case 3:
                    source_pixel.r = from_data[from_idx];
                    source_pixel.g = from_data[from_idx + 1];
                    source_pixel.b = from_data[from_idx + 2];
                    break;
                case 4:
                    source_pixel.r = from_data[from_idx];
                    source_pixel.g = from_data[from_idx + 1];
                    source_pixel.b = from_data[from_idx + 2];
                    source_pixel.a = from_data[from_idx + 3];
                    break;
            }

            switch (to_channels) {
                case 1:
                    to_data[to_idx] = source_pixel.r;
                    break;
                case 2:
                    to_data[to_idx] = source_pixel.r;
                    to_data[to_idx + 1] = source_pixel.a;
                    break;
                case 3:
                    to_data[to_idx] = source_pixel.r;
                    to_data[to_idx + 1] = source_pixel.g;
                    to_data[to_idx + 2] = source_pixel.b;
                    break;
                case 4:
                    to_data[to_idx] = source_pixel.r;
                    to_data[to_idx + 1] = source_pixel.g;
                    to_data[to_idx + 2] = source_pixel.b;
                    to_data[to_idx + 3] = source_pixel.a;
                    break;
            }
        }
    }

    ZEST__FREE(src->context->device->allocator, src->data);
    src->meta.channels = to_channels;
    src->meta.bytes_per_pixel = bytes_per_pixel;
    src->meta.format = new_format;
    src->meta.size = new_size;
    src->meta.stride = src->meta.width * bytes_per_pixel;
    src->data = new_image;

}

void zest_ConvertBitmapToBGRA(zest_bitmap src, zest_byte alpha_level) {
    zest_ConvertBitmap(src, zest_format_b8g8r8a8_unorm, alpha_level);
}

void zest_ConvertBitmapToRGBA(zest_bitmap src, zest_byte alpha_level) {
    zest_ConvertBitmap(src, zest_format_r8g8b8a8_unorm, alpha_level);
}

void zest_ConvertBGRAToRGBA(zest_bitmap  src) {
    if (src->meta.channels != 4)
        return;

    zest_size pos = 0;
    zest_size new_pos = 0;
    zest_byte* data = src->data;

    while (pos < src->meta.size) {
        zest_byte b = *(data + pos);
        *(data + pos) = *(data + pos + 2);
        *(data + pos + 2) = b;
        pos += 4;
    }
}

void zest_CopyWholeBitmap(zest_bitmap src, zest_bitmap dst) {
	ZEST_ASSERT_HANDLE(src);		//Not a valid src bitmap handle
	ZEST_ASSERT_HANDLE(dst);		//Not a valid dst bitmap handle
    ZEST_ASSERT(src->data && src->meta.size);

    zest_FreeBitmapData(dst);
    zest_SetText(dst->context->device->allocator, &dst->name, src->name.str);
    dst->meta.channels = src->meta.channels;
    dst->meta.height = src->meta.height;
    dst->meta.width = src->meta.width;
    dst->meta.size = src->meta.size;
    dst->meta.stride = src->meta.stride;
    dst->data = ZEST_NULL;
    dst->data = (zest_byte*)ZEST__ALLOCATE(src->context->device->allocator, src->meta.size);
    ZEST_ASSERT(dst->data);    //out of memory;
    memcpy(dst->data, src->data, src->meta.size);

}

void zest_CopyBitmap(zest_bitmap src, int from_x, int from_y, int width, int height, zest_bitmap dst, int to_x, int to_y) {
    ZEST_ASSERT(src->data, "No data was found in the source image");
	ZEST_ASSERT(dst->data, "No data was found in the destination image");
    ZEST_ASSERT(src->meta.format == dst->meta.format, "Both bitmaps must be the same format");

    if (from_x + width > src->meta.width)
        width = src->meta.width - from_x;
    if (from_y + height > src->meta.height)
        height = src->meta.height- from_y;

    if (to_x + width > dst->meta.width)
        to_x = dst->meta.width - width;
    if (to_y + height > dst->meta.height)
        to_y = dst->meta.height - height;

    if (src->data && dst->data && width > 0 && height > 0) {
        int src_row = from_y * src->meta.stride + (from_x * src->meta.bytes_per_pixel);
        int dst_row = to_y * dst->meta.stride + (to_x * dst->meta.bytes_per_pixel);
        size_t row_size = width * src->meta.bytes_per_pixel;
        int rows_copied = 0;
        while (rows_copied < height) {
            memcpy(dst->data + dst_row, src->data + src_row, row_size);
            rows_copied++;
            src_row += src->meta.stride;
            dst_row += dst->meta.stride;
        }
    }
}

zest_color_t zest_SampleBitmap(zest_bitmap image, int x, int y) {
    ZEST_ASSERT(image->data);

    size_t offset = y * image->meta.stride + (x * image->meta.channels);
    zest_color_t c = ZEST__ZERO_INIT(zest_color_t);
    if (offset < image->meta.size) {
        c.r = *(image->data + offset);

        if (image->meta.channels == 2) {
            c.a = *(image->data + offset + 1);
        }

        if (image->meta.channels == 3) {
            c.g = *(image->data + offset + 1);
            c.b = *(image->data + offset + 2);
        }

        if (image->meta.channels == 4) {
            c.g = *(image->data + offset + 1);
            c.b = *(image->data + offset + 2);
            c.a = *(image->data + offset + 3);
        }
    }

    return c;
}

float zest_FindBitmapRadius(zest_bitmap  image) {
    //Todo: optimise with SIMD
    ZEST_ASSERT(image->data);
    float max_radius = 0;
    for (int x = 0; x < image->meta.width; ++x) {
        for (int y = 0; y < image->meta.height; ++y) {
            zest_color_t c = zest_SampleBitmap(image, x, y);
            if (c.a) {
                max_radius = ceilf(ZEST__MAX(max_radius, zest_Distance((float)image->meta.width / 2.f, (float)image->meta.height / 2.f, (float)x, (float)y)));
            }
        }
    }
    return ceilf(max_radius);
}

void zest_DestroyBitmapArray(zest_bitmap_array_t* bitmap_array) {
    if (bitmap_array->data) {
        ZEST__FREE(bitmap_array->context->device->allocator, bitmap_array->data);
    }
    bitmap_array->data = ZEST_NULL;
    bitmap_array->size_of_array = 0;
}

zest_bitmap zest_GetImageFromArray(zest_bitmap_array_t* bitmap_array, zest_index index) {
    zest_bitmap image = zest_NewBitmap(bitmap_array->context);
    image->meta.width = bitmap_array->meta[index].width;
    image->meta.height = bitmap_array->meta[index].height;
    image->meta.channels = bitmap_array->meta[index].channels;
    image->meta.stride = bitmap_array->meta[index].width * bitmap_array->meta[index].bytes_per_pixel;
    image->data = bitmap_array->data + bitmap_array->meta[index].offset;
    return image;
}

// --Internal Texture functions
void zest__cleanup_image(zest_image image) {
	zest_context context = image->handle.context;
    context->device->platform->cleanup_image_backend(image);
    zest__release_all_global_texture_indexes(image);
    zest_map_foreach(i, image->mip_indexes) {
        zest_mip_index_collection *collection = &image->mip_indexes.data[i];
		zest_vec_free(image->handle.context->device->allocator, collection->mip_indexes);
    }
    zest_map_free(image->handle.context->device->allocator, image->mip_indexes);
    zest__remove_store_resource(image->handle.context, image->handle.value);
    if (image->default_view) {
        context->device->platform->cleanup_image_view_backend(image->default_view);
        ZEST__FREE(image->handle.context->device->allocator, image->default_view);
    }
    image->magic = 0;
}

void zest__cleanup_sampler(zest_sampler sampler) {
	zest_context context = sampler->handle.context;
    context->device->platform->cleanup_sampler_backend(sampler);
    zest__remove_store_resource(sampler->handle.context, sampler->handle.value);
    sampler->magic = 0;
}

void zest__cleanup_uniform_buffer(zest_uniform_buffer uniform_buffer) {
	zest_context context = uniform_buffer->handle.context;
    zest_ForEachFrameInFlight(fif) {
        zest_buffer buffer = uniform_buffer->buffer[fif];
        zest_FreeBuffer(buffer);
        zest_set_layout layout = (zest_set_layout)zest__get_store_resource_checked(uniform_buffer->set_layout.context, uniform_buffer->set_layout.value);
		ZEST__FREE(context->device->allocator, uniform_buffer->descriptor_set[fif]->backend);
        ZEST__FREE(context->device->allocator, uniform_buffer->descriptor_set[fif]);
    }
    context->device->platform->cleanup_uniform_buffer_backend(uniform_buffer);
    zest__remove_store_resource(context, uniform_buffer->handle.value);
}

void zest__get_format_pixel_data(zest_format format, int *channels, int *bytes_per_pixel) { 
	*channels = 0;
	*bytes_per_pixel = 0;
    switch (format) {
    case zest_format_undefined:
    case zest_format_r8_unorm:
    case zest_format_r8_snorm:
    case zest_format_r8_srgb: {
		*channels = 1;
		*bytes_per_pixel = 1;
        break;
    }
    case zest_format_r8g8_unorm:
    case zest_format_r8g8_snorm:
    case zest_format_r8g8_uint:
    case zest_format_r8g8_sint:
    case zest_format_r8g8_srgb: {
		*channels = 2;
		*bytes_per_pixel = 2;
        break;
    }
    case zest_format_r8g8b8_unorm:
    case zest_format_r8g8b8_snorm:
    case zest_format_r8g8b8_uint:
    case zest_format_r8g8b8_sint:
    case zest_format_r8g8b8_srgb:
    case zest_format_b8g8r8_unorm:
    case zest_format_b8g8r8_snorm:
    case zest_format_b8g8r8_uint:
    case zest_format_b8g8r8_sint:
    case zest_format_b8g8r8_srgb: {
		*channels = 3;
		*bytes_per_pixel = 3;
        break;
    }
    case zest_format_r8g8b8a8_unorm:
    case zest_format_r8g8b8a8_snorm:
    case zest_format_r8g8b8a8_uint:
    case zest_format_r8g8b8a8_sint:
    case zest_format_r8g8b8a8_srgb:
    case zest_format_b8g8r8a8_unorm:
    case zest_format_b8g8r8a8_snorm:
    case zest_format_b8g8r8a8_uint:
    case zest_format_b8g8r8a8_sint:
    case zest_format_b8g8r8a8_srgb: {
		*channels = 4;
		*bytes_per_pixel = 4;
        break;
    }
    case zest_format_r32_sfloat:
    case zest_format_r32_uint:
	case zest_format_r32_sint: {
		*channels = 1;
		*bytes_per_pixel = 4;
        break;
    }
    case zest_format_r32g32_sfloat:
    case zest_format_r32g32_uint:
	case zest_format_r32g32_sint: {
		*channels = 2;
		*bytes_per_pixel = 8;
        break;
    }
    case zest_format_r32g32b32_sfloat:
    case zest_format_r32g32b32_uint:
	case zest_format_r32g32b32_sint: {
		*channels = 3;
		*bytes_per_pixel = 12;
        break;
    }
    case zest_format_r32g32b32a32_sfloat:
    case zest_format_r32g32b32a32_uint:
	case zest_format_r32g32b32a32_sint: {
		*channels = 4;
		*bytes_per_pixel = 16;
        break;
    }
    default:
        break;
    }
}

zest_image_collection_handle zest_CreateImageCollection(zest_context context, zest_format format, zest_uint image_count, zest_image_collection_flags flags) {
	zest_image_collection_handle image_collection_handle = ZEST_STRUCT_LITERAL(zest_image_collection_handle, zest__add_store_resource(zest_handle_type_image_collection, context), context);
	zest_image_collection image_collection = (zest_image_collection)zest__get_store_resource_checked(context, image_collection_handle.value);
	*image_collection = ZEST__ZERO_INIT(zest_image_collection_t);
	image_collection->magic = zest_INIT_MAGIC(zest_struct_type_image_collection);
	image_collection->handle = image_collection_handle;
    image_collection->format = format;
	image_collection->flags = flags;
    zest_InitialiseBitmapArray(context, &image_collection->bitmap_array, image_count);
	return image_collection_handle;
}

zest_image_collection_handle zest_CreateImageAtlasCollection(zest_context context, zest_format format) {
	ZEST_ASSERT_TILING_FORMAT(format);	//Format not supported for tiled images that will be sampled in a shader
	zest_image_collection_handle image_collection_handle = ZEST_STRUCT_LITERAL(zest_image_collection_handle, zest__add_store_resource(zest_handle_type_image_collection, context), context);
	zest_image_collection image_collection = (zest_image_collection)zest__get_store_resource_checked(context, image_collection_handle.value);
	*image_collection = ZEST__ZERO_INIT(zest_image_collection_t);
	image_collection->magic = zest_INIT_MAGIC(zest_struct_type_image_collection);
	image_collection->handle = image_collection_handle;
    image_collection->format = format;
	image_collection->flags = zest_image_collection_flag_atlas;
	return image_collection_handle;
}

zest_atlas_region zest_AddImageAtlasPNG(zest_image_collection_handle image_collection_handle, const char *filename, const char *name) {
	zest_image_collection image_collection = (zest_image_collection)zest__get_store_resource_checked(image_collection_handle.context, image_collection_handle.value);
	zest_context context = image_collection_handle.context;
	zest_bitmap bitmap = zest_LoadPNG(context, filename);
	if (bitmap) {
		if (bitmap->meta.format != image_collection->format) {
			zest_ConvertBitmap(bitmap, image_collection->format, 255);
		}
		zest_atlas_region region = zest_NewAtlasRegion(context);
		bitmap->atlas_region = region;
		region->width = bitmap->meta.width;
		region->height = bitmap->meta.height;
		zest_map_insert(context->device->allocator, image_collection->image_bitmaps, name, bitmap);
		return region;
	}
	return NULL;
}

zest_atlas_region zest_AddImageAtlasBitmap(zest_image_collection_handle image_collection_handle, zest_bitmap bitmap, const char *name) {
	ZEST_ASSERT_HANDLE(bitmap);		//Not a valid bitmap handle
	ZEST_ASSERT(bitmap->meta.width, "Width has not been set in the bitmap");		//Not a valid bitmap handle
	ZEST_ASSERT(bitmap->meta.height, "Height has not been set in the bitmap");		//Not a valid bitmap handle
	ZEST_ASSERT(bitmap->meta.format, "Format has not been set in the bitmap");		//Not a valid bitmap handle
	zest_image_collection image_collection = (zest_image_collection)zest__get_store_resource_checked(image_collection_handle.context, image_collection_handle.value);
	zest_context context = image_collection_handle.context;
	if (bitmap) {
		if (bitmap->meta.format != image_collection->format) {
			zest_ConvertBitmap(bitmap, image_collection->format, 255);
		}
		zest_atlas_region region = zest_NewAtlasRegion(context);
		bitmap->atlas_region = region;
		region->width = bitmap->meta.width;
		region->height = bitmap->meta.height;
		zest_map_insert(context->device->allocator, image_collection->image_bitmaps, name, bitmap);
		return region;
	}
	return NULL;
}

void zest_SetImageCollectionBitmapMeta(zest_image_collection_handle image_collection_handle, zest_uint bitmap_index, zest_uint width, zest_uint height, zest_uint channels, zest_uint stride, zest_size size_in_bytes, zest_size offset) {
	zest_image_collection image_collection = (zest_image_collection)zest__get_store_resource_checked(image_collection_handle.context, image_collection_handle.value);
	ZEST_ASSERT(bitmap_index < zest_vec_size(image_collection->bitmap_array.meta));	//bitmap index is out of bounds
    zest_bitmap_array_t *bitmap_array = &image_collection->bitmap_array;
	bitmap_array->meta[bitmap_index].width = width;
	bitmap_array->meta[bitmap_index].height = height;
	bitmap_array->meta[bitmap_index].width = width;
	bitmap_array->meta[bitmap_index].channels = channels;
	bitmap_array->meta[bitmap_index].stride = stride;
	bitmap_array->meta[bitmap_index].offset = offset;
	bitmap_array->meta[bitmap_index].size = size_in_bytes;
}

void zest_SetImageCollectionPackedBorderSize(zest_image_collection_handle image_collection_handle, zest_uint border_size) {
	zest_image_collection image_collection = (zest_image_collection)zest__get_store_resource_checked(image_collection_handle.context, image_collection_handle.value);
	image_collection->packed_border_size = border_size;
}

zest_bool zest_AllocateImageCollectionBitmapArray(zest_image_collection_handle image_collection_handle) {
	zest_image_collection image_collection = (zest_image_collection)zest__get_store_resource_checked(image_collection_handle.context, image_collection_handle.value);
	zest_context context = image_collection_handle.context;
	if (zest_vec_size(image_collection->bitmap_array.meta) == 0) {
		ZEST_PRINT("Nothing to allocate in the image collection."); 
		return ZEST_FALSE;
	}
	zest_size total_size = 0;
	zest_vec_foreach(i, image_collection->bitmap_array.meta) {
		zest_bitmap_meta_t *meta = &image_collection->bitmap_array.meta[i];
		total_size += meta->size;
	}
    zest_bitmap_array_t *bitmap_array = &image_collection->bitmap_array;
	if (bitmap_array->data) {
		ZEST__FREE(context->device->allocator, bitmap_array->data);
	}
	bitmap_array->total_mem_size = total_size;
    bitmap_array->data = (zest_byte*)ZEST__ALLOCATE(context->device->allocator, bitmap_array->total_mem_size);
	return ZEST_TRUE;
}

zest_bool zest_ImageCollectionCopyToBitmapArray(zest_image_collection_handle image_collection_handle, zest_uint bitmap_index, const void *src_data, zest_size src_size) {
	zest_image_collection image_collection = (zest_image_collection)zest__get_store_resource_checked(image_collection_handle.context, image_collection_handle.value);
	zest_byte *raw_bitmap_data = zest_GetImageCollectionRawBitmap(image_collection_handle, bitmap_index);
	zloc_SafeCopyBlock(image_collection->bitmap_array.data, raw_bitmap_data, src_data, src_size);
	return ZEST_TRUE;
}

zest_bitmap_array_t *zest_GetImageCollectionBitmapArray(zest_image_collection_handle image_collection_handle) {
	zest_image_collection image_collection = (zest_image_collection)zest__get_store_resource_checked(image_collection_handle.context, image_collection_handle.value);
	return &image_collection->bitmap_array;
}

zest_bitmap_meta_t zest_ImageCollectionBitmapArrayMeta(zest_image_collection_handle image_collection_handle) {
	zest_image_collection image_collection = (zest_image_collection)zest__get_store_resource_checked(image_collection_handle.context, image_collection_handle.value);
	if (zest_vec_size(image_collection->bitmap_array.meta)) {
		return image_collection->bitmap_array.meta[0];
	} else {
		return ZEST__ZERO_INIT(zest_bitmap_meta_t);
	}
}

zest_byte *zest_GetImageCollectionRawBitmap(zest_image_collection_handle image_collection_handle, zest_uint bitmap_index) {
	zest_image_collection image_collection = (zest_image_collection)zest__get_store_resource_checked(image_collection_handle.context, image_collection_handle.value);
	return zest_BitmapArrayLookUp(&image_collection->bitmap_array, bitmap_index);
}

void zest_InitialiseBitmapArray(zest_context context, zest_bitmap_array_t *images, zest_uint size_of_array) {
	images->context = context;
    zest_FreeBitmapArray(images);
    ZEST_ASSERT(size_of_array);            //must create with atleast one image in the array
    zest_vec_resize(images->context->device->allocator, images->meta, size_of_array);
    images->size_of_array = size_of_array;
}

void zest_CreateBitmapArray(zest_context context, zest_bitmap_array_t *images, int width, int height, zest_format format, zest_uint size_of_array) {
    ZEST_ASSERT(size_of_array);            //must create with atleast one image in the array
    if (images->data) {
        ZEST__FREE(context->device->allocator, images->data);
        images->data = ZEST_NULL;
    }
    images->size_of_array = size_of_array;
	images->context = context;
    zest_vec_resize(context->device->allocator, images->meta, size_of_array);
    size_t offset = 0;
	int channels, bytes_per_pixel;
	zest__get_format_pixel_data(format, &channels, &bytes_per_pixel);
	ZEST_ASSERT(channels, "Not a supported bitmap format.");
    size_t image_size = width * height * channels * bytes_per_pixel;
	int stride = bytes_per_pixel * width;
    zest_vec_foreach(i, images->meta) {
        images->meta[i] = ZEST_STRUCT_LITERAL(zest_bitmap_meta_t,
            width, height,
            channels,
			bytes_per_pixel,
            stride,
			image_size,
            offset,
			format
        );
        offset += image_size;
		images->total_mem_size += images->meta[i].size;
    }
    images->data = (zest_byte*)ZEST__ALLOCATE(context->device->allocator, images->total_mem_size);
    memset(images->data, 0, images->total_mem_size);
}

void zest_FreeBitmapArray(zest_bitmap_array_t* images) {
    if (images->data) {
        ZEST__FREE(images->context->device->allocator, images->data);
    }
    zest_vec_free(images->context->device->allocator, images->meta);
    images->data = ZEST_NULL;
    images->size_of_array = 0;
}

void zest__cleanup_image_collection(zest_image_collection image_collection) {
	ZEST_ASSERT_HANDLE(image_collection);   //Not a valid image collection handle/pointer!
	zest_map_foreach(i, image_collection->image_bitmaps) {
		zest_bitmap bitmap = image_collection->image_bitmaps.data[i];
		zest_FreeAtlasRegion(bitmap->atlas_region);
        zest_FreeBitmap(bitmap);
	}
    zest_vec_foreach(i, image_collection->layers) {
        zest_FreeBitmapData(&image_collection->layers[i]);
    }
	zest_context context = image_collection->handle.context;
    zest_uint i = 0;
    zest_vec_free(context->device->allocator, image_collection->regions);
	zest_map_free(context->device->allocator, image_collection->image_bitmaps);
    zest_vec_free(context->device->allocator, image_collection->buffer_copy_regions);
    zest_vec_free(context->device->allocator, image_collection->layers);
    zest_FreeBitmapArray(&image_collection->bitmap_array);
	if (image_collection->handle.value) {
		zest__remove_store_resource(context, image_collection->handle.value);
	}
}

void zest_FreeImageCollection(zest_image_collection_handle image_collection_handle) {
	zest_context context = image_collection_handle.context;
	zest_image_collection image_collection = (zest_image_collection)zest__get_store_resource_checked(context, image_collection_handle.value);
	zest__cleanup_image_collection(image_collection);
}

void zest_SetImageHandle(zest_atlas_region image, float x, float y) {
    if (image->handle.x == x && image->handle.y == y)
        return;
    image->handle.x = x;
    image->handle.y = y;
    zest__update_image_vertices(image);
}

void zest__update_image_vertices(zest_atlas_region image) {
    image->min.x = image->width * (0.f - image->handle.x);
    image->min.y = image->height * (0.f - image->handle.y);
    image->max.x = image->width * (1.f - image->handle.x);
    image->max.y = image->height * (1.f - image->handle.y);
}

zest_image_info_t zest_CreateImageInfo(zest_uint width, zest_uint height) {
    zest_image_info_t info = {
        {width, height, 1},
        1,
        1,
        zest_format_r8g8b8a8_unorm,
        0,
        zest_sample_count_1_bit,
        0
    };
    return info;
}

zest_image_view_type zest__get_image_view_type(zest_image image) {
    zest_image_view_type view_type;
    if (image->info.extent.depth > 1) {
        view_type = zest_image_view_type_3d;
    } else if (ZEST__FLAGGED(image->info.flags, zest_image_flag_cubemap)) {
        view_type = (image->info.layer_count > 6) ? zest_image_view_type_cube_array : zest_image_view_type_cube;
    } else {
        view_type = (image->info.layer_count > 1) ? zest_image_view_type_2d_array : zest_image_view_type_2d;
    }
    return view_type;
}

zest_image_handle zest_CreateImage(zest_context context, zest_image_info_t *create_info) {
	ZEST_ASSERT_HANDLE(context);
    ZEST_ASSERT(create_info->extent.width * create_info->extent.height * create_info->extent.depth > 0); //Image has 0 dimensions!
    ZEST_ASSERT(create_info->flags);    //You must set flags in the image info to specify how the image will be used.
                                        //For example you could use zest_image_preset_texture. Lookup the zest_image_flag_bits emum
                                        //to see all the flags available.
	zest_image_handle handle = zest__new_image(context);
    zest_image image = (zest_image)zest__get_store_resource_checked(context, handle.value);
    image->info = *create_info;
    image->info.aspect_flags = zest__determine_aspect_flag(create_info->format);
    image->info.mip_levels = create_info->mip_levels > 0 ? create_info->mip_levels : 1;
    if (ZEST__FLAGGED(image->info.flags, zest_image_flag_cubemap)) {
        ZEST_ASSERT(image->info.layer_count > 0 && image->info.layer_count % 6 == 0); // Cubemap must have layers in multiples of 6!
    }
    if (ZEST__FLAGGED(create_info->flags, zest_image_flag_generate_mipmaps) && image->info.mip_levels == 1) {
        image->info.mip_levels = (zest_uint)floor(log2(ZEST__MAX(create_info->extent.width, create_info->extent.height))) + 1;
    }
    if (!context->device->platform->create_image(context, image, image->info.layer_count, zest_sample_count_1_bit, create_info->flags)) {
        zest__cleanup_image(image);
        return ZEST__ZERO_INIT(zest_image_handle);
    }
	image->info.layout = zest_image_layout_undefined;
    if (ZEST__FLAGGED(image->info.flags, zest_image_flag_storage)) {
        context->device->platform->begin_single_time_commands(context);
        zest__transition_image_layout(image, zest_image_layout_general, 0, ZEST__ALL_MIPS, 0, ZEST__ALL_LAYERS);
        context->device->platform->end_single_time_commands(context);
    }
    zest_image_view_type view_type = zest__get_image_view_type(image);
    image->default_view = context->device->platform->create_image_view(image, view_type, image->info.mip_levels, 0, 0, image->info.layer_count, 0);
    image->default_view->handle.context = context;
    return handle;
} 

zest_image_handle zest_CreateImageWithBitmap(zest_context context, zest_bitmap bitmap, zest_image_flags flags) {
	ZEST_ASSERT_HANDLE(bitmap);	//Not a valid bitmap handle
	ZEST_ASSERT(bitmap->data);	//No data in the bitmap
	zest_image_info_t image_info = zest_CreateImageInfo(bitmap->meta.width, bitmap->meta.height);
	image_info.format = bitmap->meta.format;

	if (flags == 0) {
		image_info.flags = zest_image_preset_texture_mipmaps;
	} else {
		ZEST_ASSERT(flags & zest_image_preset_texture, "If you pass in flags to the zest_CreateImageWithBitmap function then it must at lease contain zest_image_preset_texture flags. You can leave as 0 and an image with mipmaps will be created.");
        image_info.flags = flags;
	}

	zest_image_handle image_handle = zest_CreateImage(context, &image_info);
	if (!image_handle.value) {
		ZEST_PRINT("Unable to create the image. Check for validation errors.");
		return image_handle;
	}
	if (!zest_CopyBitmapToImage(bitmap, image_handle, 0, 0, 0, 0, image_info.extent.width, image_info.extent.height)) {
		ZEST_PRINT("Unable to copy the bitmap to the image. Check for validation errors.");
	}
    zest_image image = (zest_image)zest__get_store_resource(image_handle.context, image_handle.value);
	zest_uint mip_levels = image->info.mip_levels;
    if (mip_levels > 1) {
		context->device->platform->begin_single_time_commands(context);
		ZEST_CLEANUP_ON_FALSE(zest__transition_image_layout(image, zest_image_layout_transfer_dst_optimal, 0, mip_levels, 0, 1));
        ZEST_CLEANUP_ON_FALSE(context->device->platform->generate_mipmaps(image));
		context->device->platform->end_single_time_commands(context);
    }
	return image_handle;

	cleanup:
	zest__cleanup_image(image);
    return ZEST__ZERO_INIT(zest_image_handle);
}

zest_image_handle zest_CreateImageAtlas(zest_image_collection_handle atlas_handle, zest_uint layer_width, zest_uint layer_height, zest_image_flags flags) {
    zest_image_collection atlas = (zest_image_collection)zest__get_store_resource_checked(atlas_handle.context, atlas_handle.value);
	zest_image_info_t image_info = zest_CreateImageInfo(layer_width, layer_height);
	image_info.format = atlas->format;
	zest__pack_images(atlas, layer_width, layer_height);
	image_info.layer_count = atlas->layer_count;

	if (flags == 0) {
		image_info.flags = zest_image_preset_texture_mipmaps;
	} else {
		ZEST_ASSERT(flags & zest_image_preset_texture, "If you pass in flags to the zest_CreateImageAtlas function then it must at lease contain zest_image_preset_texture. You can leave as 0 an image with mipmaps will be created.");
        image_info.flags = flags;
	}

	zest_context context = atlas->handle.context;

	zest_image_handle image_handle = zest_CreateImage(context, &image_info);

    zest_size image_size = atlas->bitmap_array.total_mem_size;

    zest_buffer staging_buffer = zest_CreateStagingBuffer(context, image_size, 0);

    if (!staging_buffer) {
        goto cleanup;
    }

    memcpy(staging_buffer->data, atlas->bitmap_array.data, atlas->bitmap_array.total_mem_size);

    zest_uint width = atlas->bitmap_array.meta[0].width;
    zest_uint height = atlas->bitmap_array.meta[0].height;

    zest_image image = (zest_image)zest__get_store_resource(image_handle.context, image_handle.value);
    zest_uint mip_levels = image->info.mip_levels;

    context->device->platform->begin_single_time_commands(context);
    ZEST_CLEANUP_ON_FALSE(zest__transition_image_layout(image, zest_image_layout_transfer_dst_optimal, 0, mip_levels, 0, atlas->layer_count));
    ZEST_CLEANUP_ON_FALSE(context->device->platform->copy_buffer_regions_to_image(context, atlas->buffer_copy_regions, staging_buffer, staging_buffer->buffer_offset, image));
	if (image->info.mip_levels > 1) {
		ZEST_CLEANUP_ON_FALSE(context->device->platform->generate_mipmaps(image));
	} else {
		ZEST_CLEANUP_ON_FALSE(zest__transition_image_layout(image, zest_image_layout_shader_read_only_optimal, 0, mip_levels, 0, atlas->layer_count));
	}
    context->device->platform->end_single_time_commands(context);

	zest_FreeBuffer(staging_buffer);

    return image_handle;

    cleanup:
	zest__cleanup_image(image);
	zest_FreeBuffer(staging_buffer);
    return ZEST__ZERO_INIT(zest_image_handle);
}

void zest_FreeImage(zest_image_handle handle) {
	zest_context context = handle.context;
    zest_image image = (zest_image)zest__get_store_resource_checked(handle.context, handle.value);
    zest_vec_push(handle.context->device->allocator, context->device->deferred_resource_freeing_list.resources[context->current_fif], image);
}

zest_image_view_create_info_t zest_CreateViewImageInfo(zest_image_handle image_handle) {
    zest_image image = (zest_image)zest__get_store_resource_checked(image_handle.context, image_handle.value);
    zest_image_view_type view_type = zest__get_image_view_type(image);
    zest_image_view_create_info_t info = {
        view_type,
        0,
        image->info.mip_levels,
        0,
        image->info.layer_count
    };
    return info;
}

zest_image_view_handle zest_CreateImageView(zest_image_handle image_handle, zest_image_view_create_info_t *create_info) {
    zest_image_view_handle view_handle = ZEST_STRUCT_LITERAL(zest_image_view_handle, zest__add_store_resource(zest_handle_type_views, image_handle.context) );
	view_handle.context = image_handle.context;
    zest_image_view *view = (zest_image_view*)zest__get_store_resource(view_handle.context, view_handle.value);
    zest_image image = (zest_image)zest__get_store_resource_checked(image_handle.context, image_handle.value);
	zest_context context = image_handle.context;
    *view = context->device->platform->create_image_view(image, create_info->view_type, create_info->level_count, 
                                    create_info->base_mip_level, create_info->base_array_layer,
									create_info->layer_count, 0);
    (*view)->handle = view_handle;
    return view_handle;
}

zest_image_view_array_handle zest_CreateImageViewsPerMip(zest_image_handle image_handle) {
    zest_image_view_array_handle view_handle = ZEST_STRUCT_LITERAL(zest_image_view_array_handle, zest__add_store_resource(zest_handle_type_view_arrays, image_handle.context) );
	view_handle.context = image_handle.context;
    zest_image_view_array *view = (zest_image_view_array*)zest__get_store_resource(view_handle.context, view_handle.value);
    zest_image image = (zest_image)zest__get_store_resource_checked(image_handle.context, image_handle.value);
    zest_image_view_type view_type = zest__get_image_view_type(image);
	zest_context context = image_handle.context;
    *view = context->device->platform->create_image_views_per_mip(image, view_type, 0, image->info.layer_count, 0);
    (*view)->handle = view_handle;
    return view_handle;
}

zest_byte* zest_BitmapArrayLookUp(zest_bitmap_array_t* bitmap_array, zest_index index) {
    ZEST_ASSERT((zest_uint)index < bitmap_array->size_of_array);	//index out of bounds
    return bitmap_array->data + bitmap_array->meta[index].offset;
}

zest_size zest_BitmapArrayLookUpSize(zest_bitmap_array_t *bitmap_array, zest_index index) {
    ZEST_ASSERT((zest_uint)index < bitmap_array->size_of_array);	//index out of bounds
    return bitmap_array->meta[index].size;
}

zest_index zest_RegionLayerIndex(zest_atlas_region image) {
    ZEST_ASSERT_HANDLE(image);  //Not a valid image handle
    return image->layer_index;
}

zest_extent2d_t zest_RegionDimensions(zest_atlas_region image) {
    ZEST_ASSERT_HANDLE(image);  //Not a valid image handle
    return ZEST_STRUCT_LITERAL(zest_extent2d_t, image->width, image->height );
}

zest_vec4 zest_ImageUV(zest_atlas_region region) {
    ZEST_ASSERT_HANDLE(region);  //Not a valid region handle
    return region->uv;
}

const zest_image_info_t *zest_ImageInfo(zest_image_handle image_handle) {
    zest_image image = (zest_image)zest__get_store_resource(image_handle.context, image_handle.value);
    return &image->info;
}

zest_uint zest_ImageDescriptorIndex(zest_image_handle image_handle, zest_binding_number_type binding_number) {
    ZEST_ASSERT(binding_number < zest_max_global_binding_number);   //Invalid binding number
    zest_image image = (zest_image)zest__get_store_resource(image_handle.context, image_handle.value);
    return image->bindless_index[binding_number];
}

int zest_ImageRawLayout(zest_image_handle image_handle) {
    zest_image image = (zest_image)zest__get_store_resource(image_handle.context, image_handle.value);
	zest_context context = image_handle.context;
    return context->device->platform->get_image_raw_layout(image);
}
//-- End Texture and Image Functions

void zest_UploadInstanceLayerData(const zest_command_list command_list, void *user_data) {
	zest_layer_handle layer_handle = *(zest_layer_handle*)user_data;
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(layer_handle.context, layer_handle.value);

    ZEST__MAYBE_REPORT(command_list->context, !ZEST_VALID_HANDLE(layer), zest_report_invalid_layer, "Error in [%s] The zest_UploadInstanceLayerData was called with invalid layer data. Pass in a valid layer or array of layers to the zest_SetPassTask function in the frame graph.", zest__frame_graph_builder->frame_graph->name);

    if (ZEST_VALID_HANDLE(layer)) {  //You must pass in the zest_layer in the user data

        if (!layer->dirty[layer->fif]) {
            return;
        }

        zest_buffer staging_buffer = layer->memory_refs[layer->fif].staging_instance_data;
        zest_buffer device_buffer = ZEST__FLAGGED(layer->flags, zest_layer_flag_manual_fif) ? layer->memory_refs[layer->fif].device_vertex_data : layer->vertex_buffer_node->storage_buffer;

        zest_buffer_uploader_t instance_upload = { 0, staging_buffer, device_buffer, 0 };

        layer->dirty[layer->fif] = 0;

        if (staging_buffer->memory_in_use && device_buffer) {
            zest_AddCopyCommand(&instance_upload, staging_buffer, device_buffer, device_buffer->buffer_offset);
        } else {
            return;
        }

        zest_uint vertex_size = zest_vec_size(instance_upload.buffer_copies);

        zest_cmd_UploadBuffer(command_list, &instance_upload);
    }
}

//-- Draw Layers
//-- internal
zest_layer_instruction_t zest__layer_instruction() {
    zest_layer_instruction_t instruction = ZEST__ZERO_INIT(zest_layer_instruction_t);
    instruction.pipeline_template = ZEST_NULL;
    return instruction;
}

void zest__reset_instance_layer_drawing(zest_layer layer) {
    zest_vec_clear(layer->draw_instructions[layer->fif]);
    layer->memory_refs[layer->fif].staging_instance_data->memory_in_use = 0;
    layer->current_instruction = zest__layer_instruction();
    layer->memory_refs[layer->fif].instance_count = 0;
    layer->memory_refs[layer->fif].instance_ptr = layer->memory_refs[layer->fif].staging_instance_data->data;
    layer->memory_refs[layer->fif].staging_instance_data->memory_in_use = 0;
}

void zest_ResetInstanceLayerDrawing(zest_layer_handle layer_handle) {
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(layer_handle.context, layer_handle.value);
    zest_vec_clear(layer->draw_instructions[layer->fif]);
    layer->memory_refs[layer->fif].staging_instance_data->memory_in_use = 0;
    layer->current_instruction = zest__layer_instruction();
    layer->memory_refs[layer->fif].instance_count = 0;
    layer->memory_refs[layer->fif].instance_ptr = layer->memory_refs[layer->fif].staging_instance_data->data;
    layer->memory_refs[layer->fif].staging_instance_data->memory_in_use = 0;
}

zest_uint zest_GetInstanceLayerCount(zest_layer_handle layer_handle) {
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(layer_handle.context, layer_handle.value);
    return layer->memory_refs[layer->fif].instance_count;
}

zest_bool zest__grow_instance_buffer(zest_layer layer, zest_size type_size, zest_size minimum_size) {
    zest_bool grown = 0;
    if (ZEST__FLAGGED(layer->flags, zest_layer_flag_manual_fif)) {
		grown = zest_GrowBuffer(&layer->memory_refs[layer->fif].staging_instance_data, type_size, minimum_size);
        zest_GrowBuffer(&layer->memory_refs[layer->fif].device_vertex_data, type_size, minimum_size);
		layer->memory_refs[layer->fif].staging_instance_data = layer->memory_refs[layer->fif].staging_instance_data;
        if (ZEST__FLAGGED(layer->flags, zest_layer_flag_using_global_bindless_layout) && layer->memory_refs[layer->fif].device_vertex_data->array_index != ZEST_INVALID) {
            zest_buffer instance_buffer = layer->memory_refs[layer->fif].device_vertex_data;
            zest_uint array_index = instance_buffer->array_index;
			zest_context context = layer->handle.context;
			context->device->platform->update_bindless_buffer_descriptor(zest_storage_buffer_binding, array_index, instance_buffer, layer->bindless_set);
        }
    } else {
		grown = zest_GrowBuffer(&layer->memory_refs[layer->fif].staging_instance_data, type_size, minimum_size);
		layer->memory_refs[layer->fif].staging_instance_data = layer->memory_refs[layer->fif].staging_instance_data;
    }
    return grown;
}

void zest__cleanup_layer(zest_layer layer) {
	zest_ForEachFrameInFlight(fif) {
		zest_FreeBuffer(layer->memory_refs[fif].device_vertex_data);
		zest_FreeBuffer(layer->memory_refs[fif].staging_vertex_data);
		zest_FreeBuffer(layer->memory_refs[fif].staging_index_data);
		zest_vec_free(layer->handle.context->device->allocator, layer->draw_instructions[fif]);
	}
    zest__remove_store_resource(layer->handle.context, layer->handle.value);
}

void zest__reset_mesh_layer_drawing(zest_layer layer) {
    zest_vec_clear(layer->draw_instructions[layer->fif]);
    layer->memory_refs[layer->fif].staging_vertex_data->memory_in_use = 0;
    layer->memory_refs[layer->fif].staging_index_data->memory_in_use = 0;
    layer->current_instruction = zest__layer_instruction();
    layer->memory_refs[layer->fif].index_count = 0;
    layer->memory_refs[layer->fif].vertex_count = 0;
    layer->memory_refs[layer->fif].vertex_ptr = layer->memory_refs[layer->fif].staging_vertex_data->data;
    layer->memory_refs[layer->fif].index_ptr = (zest_uint*)layer->memory_refs[layer->fif].staging_index_data->data;
}

void zest__start_instance_instructions(zest_layer layer) {
    layer->current_instruction.start_index = layer->memory_refs[layer->fif].instance_count ? layer->memory_refs[layer->fif].instance_count : 0;
}

void zest__set_layer_push_constants(zest_layer layer, void *push_constants, zest_size size) {
    ZEST_ASSERT(size <= 128);   //Push constant size must not exceed 128 bytes
    memcpy(layer->current_instruction.push_constant, push_constants, size);
}

void zest_StartInstanceInstructions(zest_layer_handle layer_handle) {
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(layer_handle.context, layer_handle.value);
    layer->current_instruction.start_index = layer->memory_refs[layer->fif].instance_count ? layer->memory_refs[layer->fif].instance_count : 0;
}

void zest_ResetLayer(zest_layer_handle layer_handle) {
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(layer_handle.context, layer_handle.value);
    layer->fif = (layer->fif + 1) % ZEST_MAX_FIF;
}

void zest_ResetInstanceLayer(zest_layer_handle layer_handle) {
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(layer_handle.context, layer_handle.value);
    ZEST_ASSERT(ZEST__FLAGGED(layer->flags, zest_layer_flag_manual_fif));   //You must have created the layer with zest_CreateFIFInstanceLayer
                                                                            //if you want to manually reset the layer
    layer->prev_fif = layer->fif;
    layer->fif = (layer->fif + 1) % ZEST_MAX_FIF;
    layer->dirty[layer->fif] = 1;
    zest__reset_instance_layer_drawing(layer);
}

void zest__start_mesh_instructions(zest_layer layer) {
    layer->current_instruction.start_index = layer->memory_refs[layer->fif].index_count ? layer->memory_refs[layer->fif].index_count : 0;
}

void zest__end_instance_instructions(zest_layer layer) {
	zest_context context = layer->handle.context;
    if (ZEST__NOT_FLAGGED(layer->flags, zest_layer_flag_manual_fif)) {
        layer->fif = context->current_fif;
    }
    if (layer->current_instruction.total_instances) {
        layer->last_draw_mode = zest_draw_mode_none;
        zest_vec_push(context->device->allocator, layer->draw_instructions[layer->fif], layer->current_instruction);
        layer->current_instruction.total_instances = 0;
        layer->current_instruction.start_index = 0;
    }
    else if (layer->current_instruction.draw_mode == zest_draw_mode_viewport) {
        zest_vec_push(context->device->allocator, layer->draw_instructions[layer->fif], layer->current_instruction);
        layer->last_draw_mode = zest_draw_mode_none;
    }
    layer->memory_refs[layer->fif].staging_instance_data->memory_in_use = layer->memory_refs[layer->fif].instance_count * layer->instance_struct_size;
}

void zest_EndInstanceInstructions(zest_layer_handle layer_handle) {
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(layer_handle.context, layer_handle.value);
    zest__end_instance_instructions(layer);
}

zest_bool zest_MaybeEndInstanceInstructions(zest_layer_handle layer_handle) {
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(layer_handle.context, layer_handle.value);
    if (layer->current_instruction.total_instances) {
        layer->last_draw_mode = zest_draw_mode_none;
        zest_vec_push(layer->handle.context->device->allocator, layer->draw_instructions[layer->fif], layer->current_instruction);
        layer->current_instruction.total_instances = 0;
        layer->current_instruction.start_index = 0;
    }
    else if (layer->current_instruction.draw_mode == zest_draw_mode_viewport) {
        zest_vec_push(layer->handle.context->device->allocator, layer->draw_instructions[layer->fif], layer->current_instruction);
        layer->last_draw_mode = zest_draw_mode_none;
    }
    return 1;
}

zest_size zest_GetLayerInstanceSize(zest_layer_handle layer_handle) {
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(layer_handle.context, layer_handle.value);
	return layer->memory_refs[layer->fif].instance_count * layer->instance_struct_size;
}

void zest__end_mesh_instructions(zest_layer layer) {
    if (layer->current_instruction.total_indexes) {
        layer->last_draw_mode = zest_draw_mode_none;
        zest_vec_push(layer->handle.context->device->allocator, layer->draw_instructions[layer->fif], layer->current_instruction);

        layer->memory_refs[layer->fif].staging_index_data->memory_in_use += layer->current_instruction.total_indexes * sizeof(zest_uint);
        layer->memory_refs[layer->fif].staging_vertex_data->memory_in_use += layer->current_instruction.total_indexes * layer->vertex_struct_size;
        layer->current_instruction.total_indexes = 0;
        layer->current_instruction.start_index = 0;
    }
    else if (layer->current_instruction.draw_mode == zest_draw_mode_viewport) {
        zest_vec_push(layer->handle.context->device->allocator, layer->draw_instructions[layer->fif], layer->current_instruction);

        layer->last_draw_mode = zest_draw_mode_none;
    }
}

void zest__update_instance_layer_resolution(zest_layer layer) {
	zest_context context = layer->handle.context;
    layer->viewport.width = (float)zest_GetSwapChainExtent(context).width;
    layer->viewport.height = (float)zest_GetSwapChainExtent(context).height;
    layer->screen_scale.x = layer->viewport.width / layer->layer_size.x;
    layer->screen_scale.y = layer->viewport.height / layer->layer_size.y;
    layer->scissor.extent.width = (zest_uint)layer->viewport.width;
    layer->scissor.extent.height = (zest_uint)layer->viewport.height;
}

//Start general instance layer functionality -----
void *zest_NextInstance(zest_layer layer) {
    zest_byte* instance_ptr = (zest_byte*)layer->memory_refs[layer->fif].instance_ptr + layer->instance_struct_size;
    //Make sure we're not trying to write outside of the buffer range
    ZEST_ASSERT(instance_ptr >= (zest_byte*)layer->memory_refs[layer->fif].staging_instance_data->data && instance_ptr <= (zest_byte*)layer->memory_refs[layer->fif].staging_instance_data->end);
    if (instance_ptr == layer->memory_refs[layer->fif].staging_instance_data->end) {
        if (zest__grow_instance_buffer(layer, layer->instance_struct_size, 0)) {
            layer->memory_refs[layer->fif].instance_count++;
            instance_ptr = (zest_byte*)layer->memory_refs[layer->fif].staging_instance_data->data;
            instance_ptr += layer->memory_refs[layer->fif].instance_count * layer->instance_struct_size;
			layer->current_instruction.total_instances++;
        }
        else {
            instance_ptr = instance_ptr - layer->instance_struct_size;
        }
    }
    else {
		layer->current_instruction.total_instances++;
        layer->memory_refs[layer->fif].instance_count++;
    }
    layer->memory_refs[layer->fif].instance_ptr = instance_ptr;
	return (zest_byte *)instance_ptr - layer->instance_struct_size;
}

zest_draw_buffer_result zest_DrawInstanceBuffer(zest_layer_handle layer_handle, void *src, zest_uint amount) {
	zest_context context = layer_handle.context;
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(context, layer_handle.value);
    if (!amount) return zest_draw_buffer_result_ok;
    zest_draw_buffer_result result = zest_draw_buffer_result_ok;
    zest_size size_in_bytes_to_copy = amount * layer->instance_struct_size;
    zest_byte *instance_ptr = (zest_byte *)layer->memory_refs[layer->fif].instance_ptr;
    int fif = context->current_fif;
    ptrdiff_t diff = (zest_byte *)layer->memory_refs[layer->fif].staging_instance_data->end - (instance_ptr + size_in_bytes_to_copy);
    if (diff <= 0) {
        if (zest__grow_instance_buffer(layer, layer->instance_struct_size, (layer->memory_refs[layer->fif].instance_count * layer->instance_struct_size) + size_in_bytes_to_copy)) {
            instance_ptr = (zest_byte *)layer->memory_refs[layer->fif].staging_instance_data->data;
            instance_ptr += layer->memory_refs[layer->fif].instance_count * layer->instance_struct_size;
            diff = (zest_byte *)layer->memory_refs[layer->fif].staging_instance_data->end - instance_ptr;
            result = zest_draw_buffer_result_buffer_grew;
        }
        else {
            return zest_draw_buffer_result_failed_to_grow;
        }
    }
    memcpy(instance_ptr, src, size_in_bytes_to_copy);
    layer->memory_refs[layer->fif].instance_count += amount;
    layer->current_instruction.total_instances += amount;
    instance_ptr += size_in_bytes_to_copy;
    layer->memory_refs[layer->fif].instance_ptr = instance_ptr;
    return result;
}

void zest_DrawInstanceInstruction(zest_layer_handle layer_handle, zest_uint amount) {
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(layer_handle.context, layer_handle.value);
    layer->memory_refs[layer->fif].instance_count += amount;
    layer->current_instruction.total_instances += amount;
    zest_size size_in_bytes_to_draw = amount * layer->instance_struct_size;
    zest_byte* instance_ptr = (zest_byte*)layer->memory_refs[layer->fif].instance_ptr;
    instance_ptr += size_in_bytes_to_draw;
    layer->memory_refs[layer->fif].instance_ptr = instance_ptr;
}

// End general instance layer functionality -----

//-- Draw Layers API
zest_layer_handle zest__new_layer(zest_context context, zest_layer *out_layer) {
    zest_layer_handle handle = ZEST_STRUCT_LITERAL(zest_layer_handle, zest__add_store_resource(zest_handle_type_layers, context) );
	handle.context = context;
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(context, handle.value);
    *layer = ZEST__ZERO_INIT(zest_layer_t);
    layer->magic = zest_INIT_MAGIC(zest_struct_type_layer);
    layer->handle = handle;
    *out_layer = layer;
    return handle;
}

void zest_FreeLayer(zest_layer_handle layer_handle) {
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(layer_handle.context, layer_handle.value);
	zest_context context = layer_handle.context;
    zest_vec_push(context->device->allocator, context->device->deferred_resource_freeing_list.resources[context->current_fif], layer);
}

void zest_SetLayerViewPort(zest_layer_handle layer_handle, int x, int y, zest_uint scissor_width, zest_uint scissor_height, float viewport_width, float viewport_height) {
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(layer_handle.context, layer_handle.value);
    layer->scissor = zest_CreateRect2D(scissor_width, scissor_height, x, y);
    layer->viewport = zest_CreateViewport((float)x, (float)y, viewport_width, viewport_height, 0.f, 1.f);
}

void zest_SetLayerScissor(zest_layer_handle layer_handle, int offset_x, int offset_y, zest_uint scissor_width, zest_uint scissor_height) {
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(layer_handle.context, layer_handle.value);
    layer->scissor = zest_CreateRect2D(scissor_width, scissor_height, offset_x, offset_y);
}

void zest_SetLayerSizeToSwapchain(zest_layer_handle layer_handle) {
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(layer_handle.context, layer_handle.value);
	zest_swapchain swapchain = layer_handle.context->swapchain;
    ZEST_ASSERT_HANDLE(swapchain);	//Not a valid swapchain handle!
    layer->scissor = zest_CreateRect2D((zest_uint)swapchain->size.width, (zest_uint)swapchain->size.height, 0, 0);
    layer->viewport = zest_CreateViewport(0.f, 0.f, (float)swapchain->size.width, (float)swapchain->size.height, 0.f, 1.f);

}

void zest_SetLayerSize(zest_layer_handle layer_handle, float width, float height) {
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(layer_handle.context, layer_handle.value);
    layer->layer_size.x = width;
    layer->layer_size.y = height;
}

void zest_SetLayerColor(zest_layer_handle layer_handle, zest_byte red, zest_byte green, zest_byte blue, zest_byte alpha) {
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(layer_handle.context, layer_handle.value);
    layer->current_color = zest_ColorSet(red, green, blue, alpha);
}

void zest_SetLayerColorf(zest_layer_handle layer_handle, float red, float green, float blue, float alpha) {
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(layer_handle.context, layer_handle.value);
    layer->current_color = zest_ColorSet((zest_byte)(red * 255.f), (zest_byte)(green * 255.f), (zest_byte)(blue * 255.f), (zest_byte)(alpha * 255.f));
}

void zest_SetLayerIntensity(zest_layer_handle layer_handle, float value) {
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(layer_handle.context, layer_handle.value);
    layer->intensity = value;
}

void zest_SetLayerChanged(zest_layer_handle layer_handle) {
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(layer_handle.context, layer_handle.value);
    zest_ForEachFrameInFlight(i) {
        layer->dirty[i] = 1;
    }
}

zest_bool zest_LayerHasChanged(zest_layer_handle layer_handle) {
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(layer_handle.context, layer_handle.value);
    return layer->dirty[layer->fif];
}

void zest_SetLayerUserData(zest_layer_handle layer_handle, void *data) {
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(layer_handle.context, layer_handle.value);
    layer->user_data = data;
}

void zest_UploadLayerStagingData(zest_layer_handle layer_handle, const zest_command_list command_list) {
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(layer_handle.context, layer_handle.value);

    ZEST__MAYBE_REPORT(layer_handle.context, !ZEST_VALID_HANDLE(layer), zest_report_invalid_layer, "Error in [%s] The zest_UploadLayerStagingData was called with invalid layer data. Pass in a valid layer or array of layers to the zest_SetPassTask function in the frame graph.", zest__frame_graph_builder->frame_graph->name);

    if (ZEST_VALID_HANDLE(layer)) {  //You must pass in the zest_layer in the user data

        if (!layer->dirty[layer->fif]) {
            return;
        }

        zest_buffer staging_buffer = layer->memory_refs[layer->fif].staging_instance_data;
        zest_buffer device_buffer = ZEST__FLAGGED(layer->flags, zest_layer_flag_manual_fif) ? layer->memory_refs[layer->fif].device_vertex_data : layer->vertex_buffer_node->storage_buffer;

        zest_buffer_uploader_t instance_upload = { 0, staging_buffer, device_buffer, 0 };

        layer->dirty[layer->fif] = 0;

        if (staging_buffer->memory_in_use) {
            zest_AddCopyCommand(&instance_upload, staging_buffer, device_buffer, device_buffer->buffer_offset);
        } else {
            return;
        }

        zest_uint vertex_size = zest_vec_size(instance_upload.buffer_copies);

        zest_cmd_UploadBuffer(command_list, &instance_upload);
    }
}

zest_buffer zest_GetLayerResourceBuffer(zest_layer_handle layer_handle) {
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(layer_handle.context, layer_handle.value);
    if (ZEST__FLAGGED(layer->flags, zest_layer_flag_manual_fif)) {
        return layer->memory_refs[layer->fif].device_vertex_data;
    } else if(ZEST_VALID_HANDLE(layer->vertex_buffer_node)) {
        ZEST_ASSERT_HANDLE(layer->vertex_buffer_node); //Layer does not have a valid resource node. 
                                                      //Make sure you add it to the frame graph
        ZEST__MAYBE_REPORT(layer_handle.context, layer->vertex_buffer_node->reference_count == 0, zest_report_resource_culled, "zest_GetLayerResourceBuffer was called for resourcee [%s] that has been culled. Passes will be culled (and therefore their transient resources will not be created) if they have no outputs and therefore deemed as unnecessary and also bear in mind that passes in the chain may also be culled.", layer->vertex_buffer_node->name);   
        return layer->vertex_buffer_node->storage_buffer;
    } else {
        ZEST__REPORT(layer_handle.context, zest_report_resource_culled, "zest_GetLayerResourceBuffer was called for layer [%s] but the layer doesn't have a resource node. Make sure that you add the layer to the frame graph with zest_AddInstanceLayerBufferResource", layer->name);   
    }
    return NULL;
}

zest_buffer zest_GetLayerStagingVertexBuffer(zest_layer_handle layer_handle) {
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(layer_handle.context, layer_handle.value);
    return layer->memory_refs[layer->fif].staging_instance_data;
}

zest_buffer zest_GetLayerStagingIndexBuffer(zest_layer_handle layer_handle) {
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(layer_handle.context, layer_handle.value);
    return layer->memory_refs[layer->fif].staging_index_data;
}

void zest_DrawInstanceLayer(const zest_command_list command_list, void *user_data) {
    zest_layer_handle layer_handle = *(zest_layer_handle*)user_data;
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(layer_handle.context, layer_handle.value);

    if (!layer->vertex_buffer_node) return; //It could be that the frame graph culled the pass because it was unreferenced or disabled
    if (!layer->vertex_buffer_node->storage_buffer) return;
	zest_buffer device_buffer = layer->vertex_buffer_node->storage_buffer;
	zest_cmd_BindVertexBuffer(command_list, 0, 1, device_buffer);

    bool has_instruction_view_port = false;
    zest_vec_foreach(i, layer->draw_instructions[layer->fif]) {
        zest_layer_instruction_t* current = &layer->draw_instructions[layer->fif][i];

        if (current->draw_mode == zest_draw_mode_viewport) {
			zest_cmd_ViewPort(command_list, &current->viewport);
			zest_cmd_Scissor(command_list, &current->scissor);
            has_instruction_view_port = true;
            continue;
        } else if(!has_instruction_view_port) {
			zest_cmd_ViewPort(command_list, &layer->viewport);
			zest_cmd_Scissor(command_list, &layer->scissor);
        }

        ZEST_ASSERT(current->shader_resources.value); //Shader resources handle must be set in the instruction

        zest_pipeline pipeline = zest_PipelineWithTemplate(current->pipeline_template, command_list);
        if (pipeline) {
			zest_cmd_BindPipelineShaderResource(command_list, pipeline, current->shader_resources);
        } else {
            continue;
        }

		zest_cmd_SendPushConstants(command_list, pipeline, current->push_constant);

		zest_cmd_Draw(command_list, 6, current->total_instances, 0, current->start_index);
    }
    if (ZEST__NOT_FLAGGED(layer->flags, zest_layer_flag_manual_fif)) {
		zest__reset_instance_layer_drawing(layer);
    }
}
//-- End Draw Layers

//-- Start Instance Drawing API
zest_layer_handle zest_CreateInstanceLayer(zest_context context, const char* name, zest_size type_size) {
    zest_layer_builder_t builder = zest_NewInstanceLayerBuilder(context, type_size);
    return zest_FinishInstanceLayer(name, &builder);
}

zest_layer_handle zest_CreateFIFInstanceLayer(zest_context context, const char* name, zest_size type_size, zest_uint id) {
    zest_layer_builder_t builder = zest_NewInstanceLayerBuilder(context, type_size);
    builder.initial_count = 1000;
    zest_layer_handle layer_handle = zest_FinishInstanceLayer(name, &builder);
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(layer_handle.context, layer_handle.value);
    zest_ForEachFrameInFlight(fif) {
		zest_buffer_info_t buffer_info = zest_CreateBufferInfo(zest_buffer_type_vertex, zest_memory_usage_gpu_only);
		buffer_info.unique_id = id;
		buffer_info.frame_in_flight = fif;
        layer->memory_refs[fif].device_vertex_data = zest_CreateBuffer(context, layer->memory_refs[fif].staging_instance_data->size, &buffer_info);
    }
    ZEST__FLAG(layer->flags, zest_layer_flag_manual_fif);
    return layer_handle;
}

zest_layer_builder_t zest_NewInstanceLayerBuilder(zest_context context, zest_size type_size) {
    zest_layer_builder_t builder = { context, (zest_uint)type_size, 1000 };
    return builder;
}

zest_layer_handle zest_FinishInstanceLayer(const char *name, zest_layer_builder_t *builder) {
    zest_layer_handle layer_handle = zest__create_instance_layer(builder->context, name, builder->type_size, builder->initial_count);
    return layer_handle;
}

void zest__initialise_instance_layer(zest_context context, zest_layer layer, zest_size type_size, zest_uint instance_pool_size) {
    layer->current_color.r = 255;
    layer->current_color.g = 255;
    layer->current_color.b = 255;
    layer->current_color.a = 255;
    layer->intensity = 1;
    layer->layer_size = zest_Vec2Set1(1.f);
    layer->instance_struct_size = type_size;

    zest_buffer_info_t staging_buffer_info = zest_CreateBufferInfo(zest_buffer_type_staging, zest_memory_usage_cpu_to_gpu);
    zest_ForEachFrameInFlight(fif) {
        staging_buffer_info.frame_in_flight = fif;
		layer->memory_refs[fif].staging_instance_data = zest_CreateBuffer(context, type_size * instance_pool_size, &staging_buffer_info);
		layer->memory_refs[fif].instance_ptr = layer->memory_refs[fif].staging_instance_data->data;
		layer->memory_refs[fif].staging_instance_data = layer->memory_refs[fif].staging_instance_data;
        layer->memory_refs[fif].instance_count = 0;
        zest_vec_clear(layer->draw_instructions[fif]);
        layer->memory_refs[fif].staging_instance_data->memory_in_use = 0;
        layer->current_instruction = zest__layer_instruction();
        layer->memory_refs[fif].instance_count = 0;
        layer->memory_refs[fif].instance_ptr = layer->memory_refs[fif].staging_instance_data->data;
    }

    layer->scissor = zest_CreateRect2D(zest_ScreenWidth(context), zest_ScreenHeight(context), 0, 0);
    layer->viewport = zest_CreateViewport(0.f, 0.f, zest_ScreenWidthf(context), zest_ScreenHeightf(context), 0.f, 1.f);
}

//-- Start Instance Drawing API
void zest_SetInstanceDrawing(zest_layer_handle layer_handle, zest_shader_resources_handle handle, zest_pipeline_template pipeline) {
	ZEST_ASSERT(layer_handle.context == handle.context);	//Layer and shader resource context must match
	zest_context context = layer_handle.context;
    zest_shader_resources shader_resources = (zest_shader_resources)zest__get_store_resource_checked(context, handle.value);
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(context, layer_handle.value);
    zest__end_instance_instructions(layer);
    zest__start_instance_instructions(layer);
    layer->current_instruction.pipeline_template = pipeline;

    //The number of shader resources must match the number of descriptor layouts set in the pipeline.
    ZEST_ASSERT(zest_vec_size(shader_resources->sets[context->current_fif]) == zest_vec_size(pipeline->set_layouts));
	layer->current_instruction.shader_resources = handle;
    layer->current_instruction.draw_mode = zest_draw_mode_instance;
    layer->last_draw_mode = zest_draw_mode_instance;
}

void zest_SetLayerDrawingViewport(zest_layer_handle layer_handle, int x, int y, zest_uint scissor_width, zest_uint scissor_height, float viewport_width, float viewport_height) {
	zest_context context = layer_handle.context;
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(context, layer_handle.value);
    zest__end_instance_instructions(layer);
    zest__start_instance_instructions(layer);
    layer->current_instruction.scissor = zest_CreateRect2D(scissor_width, scissor_height, x, y);
    layer->current_instruction.viewport = zest_CreateViewport((float)x, (float)y, viewport_width, viewport_height, 0.f, 1.f);
    layer->current_instruction.draw_mode = zest_draw_mode_viewport;
}

void zest_SetLayerPushConstants(zest_layer_handle layer_handle, void *push_constants, zest_size size) {
	zest_context context = layer_handle.context;
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(context, layer_handle.value);
	zest__set_layer_push_constants(layer, push_constants, size);
}

//-- Start Mesh Drawing API
void zest__initialise_mesh_layer(zest_context context, zest_layer mesh_layer, zest_size vertex_struct_size, zest_size initial_vertex_capacity) {
    mesh_layer->current_color.r = 255;
    mesh_layer->current_color.g = 255;
    mesh_layer->current_color.b = 255;
    mesh_layer->current_color.a = 255;
    mesh_layer->intensity = 1;
    mesh_layer->layer_size = zest_Vec2Set1(1.f);
    mesh_layer->vertex_struct_size = vertex_struct_size;

    zest_buffer_info_t staging_buffer_info = zest_CreateBufferInfo(zest_buffer_type_staging, zest_memory_usage_cpu_to_gpu);

    zest_ForEachFrameInFlight(fif) {
        mesh_layer->memory_refs[fif].index_count = 0;
        mesh_layer->memory_refs[fif].index_position = 0;
        mesh_layer->memory_refs[fif].last_index = 0;
        mesh_layer->memory_refs[fif].vertex_count = 0;
		mesh_layer->memory_refs[fif].staging_vertex_data = zest_CreateBuffer(context, initial_vertex_capacity, &staging_buffer_info);
		mesh_layer->memory_refs[fif].staging_index_data = zest_CreateBuffer(context, initial_vertex_capacity, &staging_buffer_info);
		mesh_layer->memory_refs[fif].vertex_ptr = mesh_layer->memory_refs[fif].staging_vertex_data->data;
		mesh_layer->memory_refs[fif].index_ptr = (zest_uint*)mesh_layer->memory_refs[fif].staging_index_data->data;
		mesh_layer->memory_refs[fif].staging_vertex_data = mesh_layer->memory_refs[fif].staging_vertex_data;
		mesh_layer->memory_refs[fif].staging_index_data = mesh_layer->memory_refs[fif].staging_index_data;
    }

    mesh_layer->scissor = zest_CreateRect2D(zest_ScreenWidth(context), zest_ScreenHeight(context), 0, 0);
    mesh_layer->viewport = zest_CreateViewport(0.f, 0.f, zest_ScreenWidthf(context), zest_ScreenHeightf(context), 0.f, 1.f);
    zest_ForEachFrameInFlight(fif) {
        zest_vec_clear(mesh_layer->draw_instructions[fif]);
        mesh_layer->memory_refs[fif].staging_vertex_data->memory_in_use = 0;
        mesh_layer->memory_refs[fif].staging_index_data->memory_in_use = 0;
        mesh_layer->current_instruction = zest__layer_instruction();
        mesh_layer->memory_refs[fif].index_count = 0;
        mesh_layer->memory_refs[fif].vertex_count = 0;
        mesh_layer->memory_refs[fif].vertex_ptr = mesh_layer->memory_refs[fif].staging_vertex_data->data;
        mesh_layer->memory_refs[fif].index_ptr = (zest_uint*)mesh_layer->memory_refs[fif].staging_index_data->data;
    }
}

zest_buffer zest_GetVertexWriteBuffer(zest_layer_handle layer_handle) {
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(layer_handle.context, layer_handle.value);
    return layer->memory_refs[layer->fif].staging_vertex_data;
}

zest_buffer zest_GetIndexWriteBuffer(zest_layer_handle layer_handle) {
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(layer_handle.context, layer_handle.value);
    return layer->memory_refs[layer->fif].staging_index_data;
}

void zest_GrowMeshVertexBuffers(zest_layer_handle layer_handle) {
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(layer_handle.context, layer_handle.value);
    zest_size memory_in_use = zest_GetVertexWriteBuffer(layer_handle)->memory_in_use;
    zest_GrowBuffer(&layer->memory_refs[layer->fif].staging_vertex_data, layer->vertex_struct_size, memory_in_use);
}

void zest_GrowMeshIndexBuffers(zest_layer_handle layer_handle) {
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(layer_handle.context, layer_handle.value);
    zest_size memory_in_use = zest_GetVertexWriteBuffer(layer_handle)->memory_in_use;
    zest_GrowBuffer(&layer->memory_refs[layer->fif].staging_index_data, sizeof(zest_uint), memory_in_use);
}

void zest_PushVertex(zest_layer_handle layer_handle, float pos_x, float pos_y, float pos_z, float intensity, float uv_x, float uv_y, zest_color_t color, zest_uint parameters) {
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(layer_handle.context, layer_handle.value);
    zest_textured_vertex_t vertex = ZEST__ZERO_INIT(zest_textured_vertex_t);
    vertex.pos = zest_Vec3Set(pos_x, pos_y, pos_z);
    vertex.intensity = intensity;
    vertex.uv = zest_Vec2Set(uv_x, uv_y);
    vertex.color = color;
    vertex.parameters = parameters;
    zest_textured_vertex_t* vertex_ptr = (zest_textured_vertex_t*)layer->memory_refs[layer->fif].vertex_ptr;
    *vertex_ptr = vertex;
    vertex_ptr = vertex_ptr + 1;
    ZEST_ASSERT(vertex_ptr >= (zest_textured_vertex_t*)layer->memory_refs[layer->fif].staging_vertex_data->data && vertex_ptr <= (zest_textured_vertex_t*)layer->memory_refs[layer->fif].staging_vertex_data->end);
    if (vertex_ptr == layer->memory_refs[layer->fif].staging_vertex_data->end) {
        zest_bool grown = 0;
		grown = zest_GrowBuffer(&layer->memory_refs[layer->fif].staging_vertex_data, sizeof(zest_textured_vertex_t), 0);
		layer->memory_refs[layer->fif].staging_vertex_data = layer->memory_refs[layer->fif].staging_vertex_data;
        if (grown) {
            layer->memory_refs[layer->fif].vertex_count++;
            vertex_ptr = (zest_textured_vertex_t*)layer->memory_refs[layer->fif].staging_vertex_data->data;
            vertex_ptr += layer->memory_refs[layer->fif].vertex_count;
        }
        else {
            vertex_ptr = vertex_ptr - 1;
        }
    }
    else {
        layer->memory_refs[layer->fif].vertex_count++;
    }
    layer->memory_refs[layer->fif].vertex_ptr = vertex_ptr;
}

void zest_PushIndex(zest_layer_handle layer_handle, zest_uint offset) {
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(layer_handle.context, layer_handle.value);
    zest_uint index = layer->memory_refs[layer->fif].vertex_count + offset;
    zest_uint* index_ptr = (zest_uint*)layer->memory_refs[layer->fif].index_ptr;
    *index_ptr = index;
    index_ptr = index_ptr + 1;
    ZEST_ASSERT(index_ptr >= (zest_uint*)layer->memory_refs[layer->fif].staging_index_data->data && index_ptr <= (zest_uint*)layer->memory_refs[layer->fif].staging_index_data->end);
    if (index_ptr == layer->memory_refs[layer->fif].staging_index_data->end) {
        zest_bool grown = 0;
		grown = zest_GrowBuffer(&layer->memory_refs[layer->fif].staging_vertex_data, sizeof(zest_uint), 0);
		layer->memory_refs[layer->fif].staging_vertex_data = layer->memory_refs[layer->fif].staging_index_data;
        if (grown) {
            layer->memory_refs[layer->fif].index_count++;
            layer->current_instruction.total_indexes++;
            index_ptr = (zest_uint*)layer->memory_refs[layer->fif].staging_index_data->data;
            index_ptr += layer->memory_refs[layer->fif].index_count;
        }
        else {
            index_ptr = index_ptr - 1;
        }
    }
    else {
        layer->memory_refs[layer->fif].index_count++;
        layer->current_instruction.total_indexes++;
    }
    layer->memory_refs[layer->fif].index_ptr = index_ptr;
}

void zest_SetMeshDrawing(zest_layer_handle layer_handle, zest_shader_resources_handle resource_handle, zest_pipeline_template pipeline) {
	ZEST_ASSERT(layer_handle.context == resource_handle.context);	//Layer and shader resource context must match
    zest_shader_resources resources = (zest_shader_resources)zest__get_store_resource_checked(resource_handle.context, resource_handle.value);
    ZEST_ASSERT_HANDLE(resources);   //Not a valid shader resource handle
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(layer_handle.context, layer_handle.value);
    ZEST_ASSERT_HANDLE(pipeline);	//Not a valid handle!
    zest__end_mesh_instructions(layer);
    zest__start_mesh_instructions(layer);
    layer->current_instruction.pipeline_template = pipeline;
	layer->current_instruction.shader_resources = resource_handle;
    layer->current_instruction.draw_mode = zest_draw_mode_mesh;
    layer->last_draw_mode = zest_draw_mode_mesh;
}

void zest_DrawInstanceMeshLayer(const zest_command_list command_list, void *user_data) {
    zest_layer_handle layer_handle = *(zest_layer_handle*)user_data;
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(layer_handle.context, layer_handle.value);

    if (layer->vertex_data && layer->index_data) {
        zest_cmd_BindMeshVertexBuffer(command_list, layer_handle);
        zest_cmd_BindMeshIndexBuffer(command_list, layer_handle);
    } else {
        ZEST_PRINT("No Vertex/Index data found in mesh layer [%s]!", layer->name);
        return;
    }

	zest_buffer device_buffer = layer->vertex_buffer_node->storage_buffer;
	zest_cmd_BindVertexBuffer(command_list, 1, 1, device_buffer);

    bool has_instruction_view_port = false;
    zest_vec_foreach(i, layer->draw_instructions[layer->fif]) {
        zest_layer_instruction_t *current = &layer->draw_instructions[layer->fif][i];

        if (current->draw_mode == zest_draw_mode_viewport) {
			zest_cmd_Scissor(command_list, &current->scissor);
            zest_cmd_ViewPort(command_list, &current->viewport);
            has_instruction_view_port = true;
            continue;
        } else if(!has_instruction_view_port) {
			zest_cmd_Scissor(command_list, &layer->scissor);
            zest_cmd_ViewPort(command_list, &layer->viewport);
        }

        ZEST_ASSERT(current->shader_resources.value);

        zest_pipeline pipeline = zest_PipelineWithTemplate(current->pipeline_template, command_list);
        if (pipeline) {
			zest_cmd_BindPipelineShaderResource(command_list, pipeline, current->shader_resources);
        } else {
            continue;
        }

		zest_cmd_SendPushConstants(command_list, pipeline, (void*)current->push_constant);

		zest_cmd_DrawIndexed(command_list, layer->index_count, current->total_instances, 0, 0, current->start_index);
    }
    if (ZEST__NOT_FLAGGED(layer->flags, zest_layer_flag_manual_fif)) {
		zest__reset_instance_layer_drawing(layer);
    }
}
//-- End Mesh Drawing API

//-- Instanced_mesh_drawing
void zest_SetInstanceMeshDrawing(zest_layer_handle layer_handle, zest_shader_resources_handle resource_handle, zest_pipeline_template pipeline) {
    zest_shader_resources resources = (zest_shader_resources)zest__get_store_resource_checked(resource_handle.context, resource_handle.value);
    ZEST_ASSERT_HANDLE(resources);   //Not a valid shader resource handle
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(layer_handle.context, layer_handle.value);
    ZEST_ASSERT_HANDLE(pipeline);	//Not a valid handle!
    zest__end_instance_instructions(layer);
    zest__start_instance_instructions(layer);
    layer->current_instruction.pipeline_template = pipeline;
	layer->current_instruction.shader_resources = resource_handle;
    layer->current_instruction.draw_mode = zest_draw_mode_mesh_instance;
    layer->current_instruction.scissor = layer->scissor;
    layer->current_instruction.viewport = layer->viewport;
    layer->last_draw_mode = zest_draw_mode_mesh_instance;
}

void zest_PushMeshVertex(zest_mesh mesh, float pos_x, float pos_y, float pos_z, zest_color_t color) {
    zest_vertex_t vertex = { {pos_x, pos_y, pos_z}, color, {0.f, 0.f, 0.f}, 0 };
    zest_vec_push(mesh->context->device->allocator, mesh->vertices, vertex);
}

void zest_PushMeshIndex(zest_mesh mesh, zest_uint index) {
    ZEST_ASSERT(index < zest_vec_size(mesh->vertices)); //Add vertices first before triangles to make sure you're indexing vertices that exist
    zest_vec_push(mesh->context->device->allocator, mesh->indexes, index);
}

void zest_PushMeshTriangle(zest_mesh mesh, zest_uint i1, zest_uint i2, zest_uint i3) {
    ZEST_ASSERT(i1 < zest_vec_size(mesh->vertices));	//Add vertices first before triangles to make sure you're indexing vertices that exist
    ZEST_ASSERT(i2 < zest_vec_size(mesh->vertices));	//Add vertices first before triangles to make sure you're indexing vertices that exist
    ZEST_ASSERT(i3 < zest_vec_size(mesh->vertices));	//Add vertices first before triangles to make sure you're indexing vertices that exist
    zest_vec_push(mesh->context->device->allocator, mesh->indexes, i1);
    zest_vec_push(mesh->context->device->allocator, mesh->indexes, i2);
    zest_vec_push(mesh->context->device->allocator, mesh->indexes, i3);
}

zest_mesh zest_NewMesh(zest_context context) {
    zest_mesh mesh = (zest_mesh)ZEST__NEW(context->device->allocator, zest_mesh);
    *mesh = ZEST__ZERO_INIT(zest_mesh_t);
    mesh->magic = zest_INIT_MAGIC(zest_struct_type_mesh);
	mesh->context = context;
    return mesh;
}

void zest_FreeMesh(zest_mesh mesh) {
    zest_vec_free(mesh->context->device->allocator, mesh->vertices);
    zest_vec_free(mesh->context->device->allocator, mesh->indexes);
    ZEST__FREE(mesh->context->device->allocator, mesh);
}

void zest_PositionMesh(zest_mesh mesh, zest_vec3 position) {
    zest_vec_foreach(i, mesh->vertices) {
        mesh->vertices[i].pos.x += position.x;
        mesh->vertices[i].pos.y += position.y;
        mesh->vertices[i].pos.z += position.z;
    }
}

zest_matrix4 zest_RotateMesh(zest_mesh mesh, float pitch, float yaw, float roll) {
    zest_matrix4 roll_mat = zest_Matrix4RotateZ(roll);
    zest_matrix4 pitch_mat = zest_Matrix4RotateX(pitch);
    zest_matrix4 yaw_mat = zest_Matrix4RotateY(yaw);
    zest_matrix4 rotate_mat = zest_MatrixTransform(&yaw_mat, &pitch_mat);
    rotate_mat = zest_MatrixTransform(&rotate_mat, &roll_mat);
    zest_vec_foreach(i, mesh->vertices) {
        zest_vec4 pos = { mesh->vertices[i].pos.x, mesh->vertices[i].pos.y, mesh->vertices[i].pos.z, 1.f };
        pos = zest_MatrixTransformVector(&rotate_mat, pos);
        mesh->vertices[i].pos = zest_Vec3Set(pos.x, pos.y, pos.z);
    }
    return rotate_mat;
}

zest_matrix4 zest_TransformMesh(zest_mesh mesh, float pitch, float yaw, float roll, float x, float y, float z, float sx, float sy, float sz) {
    zest_matrix4 roll_mat = zest_Matrix4RotateZ(roll);
    zest_matrix4 pitch_mat = zest_Matrix4RotateX(pitch);
    zest_matrix4 yaw_mat = zest_Matrix4RotateY(yaw);
    zest_matrix4 rotate_mat = zest_MatrixTransform(&yaw_mat, &pitch_mat);
    rotate_mat = zest_MatrixTransform(&rotate_mat, &roll_mat);
    rotate_mat.v[0].w = x;
    rotate_mat.v[1].w = y;
    rotate_mat.v[2].w = z;
    rotate_mat.v[3].x = sx;
    rotate_mat.v[3].y = sy;
    rotate_mat.v[3].z = sz;
    zest_vec_foreach(i, mesh->vertices) {
        zest_vec4 pos = { mesh->vertices[i].pos.x, mesh->vertices[i].pos.y, mesh->vertices[i].pos.z, 1.f };
        pos = zest_MatrixTransformVector(&rotate_mat, pos);
        mesh->vertices[i].pos = zest_Vec3Set(pos.x, pos.y, pos.z);
    }
    return rotate_mat;
}

void zest_CalculateNormals(zest_mesh mesh) {
    // Zero out all normals first
    zest_vec_foreach(i, mesh->vertices) {
        mesh->vertices[i].normal = zest_Vec3Set(0.f, 0.f, 0.f);
    }

    // Calculate face normals and add them to each vertex of the face
    for (zest_uint i = 0; i < zest_vec_size(mesh->indexes); i += 3) {
        zest_uint i0 = mesh->indexes[i + 0];
        zest_uint i1 = mesh->indexes[i + 1];
        zest_uint i2 = mesh->indexes[i + 2];

        zest_vertex_t v0 = mesh->vertices[i0];
        zest_vertex_t v1 = mesh->vertices[i1];
        zest_vertex_t v2 = mesh->vertices[i2];

        zest_vec3 edge1 = zest_SubVec3(v1.pos, v0.pos);
        zest_vec3 edge2 = zest_SubVec3(v2.pos, v0.pos);
        zest_vec3 normal = zest_CrossProduct(edge1, edge2);

        mesh->vertices[i0].normal = zest_AddVec3(mesh->vertices[i0].normal, normal);
        mesh->vertices[i1].normal = zest_AddVec3(mesh->vertices[i1].normal, normal);
        mesh->vertices[i2].normal = zest_AddVec3(mesh->vertices[i2].normal, normal);
    }

    // Normalize all vertex normals
    zest_vec_foreach(i, mesh->vertices) {
        mesh->vertices[i].normal = zest_NormalizeVec3(mesh->vertices[i].normal);
    }
}

zest_bounding_box_t zest_NewBoundingBox() {
    zest_bounding_box_t bb = ZEST__ZERO_INIT(zest_bounding_box_t);
    bb.max_bounds = zest_Vec3Set( -9999999.f, -9999999.f, -9999999.f );
    bb.min_bounds = zest_Vec3Set( FLT_MAX, FLT_MAX, FLT_MAX );
    return bb;
}

zest_bounding_box_t zest_GetMeshBoundingBox(zest_mesh mesh) {
    zest_bounding_box_t bb = zest_NewBoundingBox();
    zest_vec_foreach(i, mesh->vertices) {
        bb.max_bounds.x = ZEST__MAX(mesh->vertices[i].pos.x, bb.max_bounds.x);
        bb.max_bounds.y = ZEST__MAX(mesh->vertices[i].pos.y, bb.max_bounds.y);
        bb.max_bounds.z = ZEST__MAX(mesh->vertices[i].pos.z, bb.max_bounds.z);
        bb.min_bounds.x = ZEST__MIN(mesh->vertices[i].pos.x, bb.min_bounds.x);
        bb.min_bounds.y = ZEST__MIN(mesh->vertices[i].pos.y, bb.min_bounds.y);
        bb.min_bounds.z = ZEST__MIN(mesh->vertices[i].pos.z, bb.min_bounds.z);
    }
    return bb;
}

void zest_SetMeshGroupID(zest_mesh mesh, zest_uint group_id) {
    zest_vec_foreach(i, mesh->vertices) {
        mesh->vertices[i].group = group_id;
    }
}

zest_size zest_MeshVertexDataSize(zest_mesh mesh) {
    return zest_vec_size(mesh->vertices) * sizeof(zest_vertex_t);
}

zest_size zest_MeshIndexDataSize(zest_mesh mesh) {
    return zest_vec_size(mesh->indexes) * sizeof(zest_uint);
}

void zest_AddMeshToMesh(zest_mesh dst_mesh, zest_mesh src_mesh) {
    zest_uint dst_vertex_size = zest_vec_size(dst_mesh->vertices);
    zest_uint src_vertex_size = zest_vec_size(src_mesh->vertices);
    zest_uint dst_index_size = zest_vec_size(dst_mesh->indexes);
    zest_uint src_index_size = zest_vec_size(src_mesh->indexes);
    zest_vec_resize(dst_mesh->context->device->allocator, dst_mesh->vertices, dst_vertex_size + src_vertex_size);
    memcpy(dst_mesh->vertices + dst_vertex_size, src_mesh->vertices, src_vertex_size * sizeof(zest_vertex_t));
    zest_vec_resize(dst_mesh->context->device->allocator, dst_mesh->indexes, dst_index_size + src_index_size);
    memcpy(dst_mesh->indexes + dst_index_size, src_mesh->indexes, src_index_size * sizeof(zest_uint));
    for (int i = dst_index_size; i != src_index_size + dst_index_size; ++i) {
        dst_mesh->indexes[i] += dst_vertex_size;
    }
}

void zest_AddMeshToLayer(zest_layer_handle layer_handle, zest_mesh src_mesh) {
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(layer_handle.context, layer_handle.value);
    zest_buffer vertex_staging_buffer = zest_CreateStagingBuffer(layer_handle.context, zest_MeshVertexDataSize(src_mesh), src_mesh->vertices);
    zest_buffer index_staging_buffer = zest_CreateStagingBuffer(layer_handle.context, zest_MeshIndexDataSize(src_mesh), src_mesh->indexes);
    layer->index_count = zest_vec_size(src_mesh->indexes);
	zest_FreeBuffer(layer->vertex_data);
	zest_FreeBuffer(layer->index_data);
	zest_context context = layer_handle.context;
	zest_buffer_info_t vertex_info = zest_CreateBufferInfo(zest_buffer_type_vertex, zest_memory_usage_gpu_only);
	zest_buffer_info_t index_info = zest_CreateBufferInfo(zest_buffer_type_index, zest_memory_usage_gpu_only);
	layer->vertex_data = zest_CreateBuffer(context, vertex_staging_buffer->size, &vertex_info);
	layer->index_data = zest_CreateBuffer(context, index_staging_buffer->size, &index_info);
	zest_BeginImmediateCommandBuffer(context);
    zest_imm_CopyBuffer(context, vertex_staging_buffer, layer->vertex_data, vertex_staging_buffer->size);
    zest_imm_CopyBuffer(context, index_staging_buffer, layer->index_data, index_staging_buffer->size);
	zest_EndImmediateCommandBuffer(context);
    zest_FreeBuffer(vertex_staging_buffer);
    zest_FreeBuffer(index_staging_buffer);
}

void zest_DrawInstancedMesh(zest_layer_handle layer_handle, float pos[3], float rot[3], float scale[3]) {
    zest_layer layer = (zest_layer)zest__get_store_resource_checked(layer_handle.context, layer_handle.value);
    ZEST_ASSERT(layer->current_instruction.draw_mode == zest_draw_mode_instance);        //Call zest_StartSpriteDrawing before calling this function

    zest_mesh_instance_t* instance = (zest_mesh_instance_t*)zest_NextInstance(layer);

    instance->pos = zest_Vec3Set(pos[0], pos[1], pos[2]);
    instance->rotation = zest_Vec3Set(rot[0], rot[1], rot[2]);
    instance->scale = zest_Vec3Set(scale[0], scale[1], scale[2]);
    instance->color = layer->current_color;
    instance->parameters = 0;

}

zest_mesh zest_CreateCylinder(zest_context context, int sides, float radius, float height, zest_color_t color, zest_bool cap) {
    float angle_increment = 2.0f * ZEST_PI / sides;

    int vertex_count = sides * 2 + (cap ? sides * 2 : 0);
    int index_count = sides * 2 + (cap ? sides : 0);

    zest_mesh mesh = zest_NewMesh(context);
    zest_vec_resize(context->device->allocator, mesh->vertices, (zest_uint)vertex_count);

    for (int i = 0; i < sides; ++i) {
        float angle = i * angle_increment;
        float x = radius * cosf(angle);
        float z = radius * sinf(angle);

        mesh->vertices[i].pos = zest_Vec3Set( x, height / 2.0f, z );
        mesh->vertices[i + sides].pos = zest_Vec3Set( x, -height / 2.0f, z );
        mesh->vertices[i].color = color;
        mesh->vertices[i + sides].color = color;
    }

    int base_index = sides * 2;

    if (cap) {
        // Generate vertices for the caps of the cylinder
        for (int i = 0; i < sides; ++i) {
            float angle = i * angle_increment;
            float x = radius * cosf(angle);
            float z = radius * sinf(angle);

            mesh->vertices[base_index + i].pos = zest_Vec3Set( x, height / 2.0f, z);
            mesh->vertices[base_index + sides + i].pos = zest_Vec3Set( x, -height / 2.0f, z);
			mesh->vertices[base_index + i].color = color;
			mesh->vertices[base_index + i + sides].color = color;
        }

        // Generate indices for the caps of the cylinder
        for (int i = 0; i < sides - 2; ++i) {
			zest_PushMeshIndex(mesh, base_index );
			zest_PushMeshIndex(mesh, base_index + i + 2);
			zest_PushMeshIndex(mesh, base_index + i + 1);

			zest_PushMeshIndex(mesh, base_index + sides);
			zest_PushMeshIndex(mesh, base_index + sides + i + 1);
			zest_PushMeshIndex(mesh, base_index + sides + i + 2);
        }
    }

    // Generate indices for the sides of the cylinder
    for (int i = 0; i < sides; ++i) {
        int next = (i + 1) % sides;

        zest_PushMeshIndex(mesh, i);
        zest_PushMeshIndex(mesh, (i + 1) % sides);
        zest_PushMeshIndex(mesh, i + sides);

        zest_PushMeshIndex(mesh, i + sides);
        zest_PushMeshIndex(mesh, (i + 1) % sides);
        zest_PushMeshIndex(mesh, (i + 1) % sides + sides );
    }

    zest_CalculateNormals(mesh);
    
    return mesh;
}

zest_mesh zest_CreateCone(zest_context context, int sides, float radius, float height, zest_color_t color) {
    // Calculate the angle between each side
    float angle_increment = 2.0f * ZEST_PI / sides;

    zest_mesh mesh = zest_NewMesh(context);
    zest_vec_resize(context->device->allocator, mesh->vertices, (zest_uint)sides * 2 + 2);

    // Generate vertices for the base of the cone
    for (int i = 0; i < sides; ++i) {
        float angle = i * angle_increment;
        float x = radius * cosf(angle);
        float z = radius * sinf(angle);
        mesh->vertices[i].pos = zest_Vec3Set( x, 0.0f, z);
        mesh->vertices[i].color = color;
    }

    // Generate the vertex for the tip of the cone
    mesh->vertices[sides].pos = zest_Vec3Set( 0.0f, height, 0.0f);
    mesh->vertices[sides].color = color;

    // Generate indices for the sides of the cone
    for (int i = 0; i < sides; ++i) {
        zest_PushMeshTriangle(mesh, i, sides, (i + 1) % sides);
    }

    int base_index = sides + 1;

    //Base center vertex
    mesh->vertices[base_index].pos = zest_Vec3Set( 0.0f, 0.0f, 0.0f);
    mesh->vertices[base_index].color = color;

    // Generate another set of vertices for the base of the cone
    for (int i = base_index + 1; i < base_index + sides + 1; ++i) {
        float angle = i * angle_increment;
        float x = radius * cosf(angle);
        float z = radius * sinf(angle);
        mesh->vertices[i].pos = zest_Vec3Set( x, 0.0f, z);
        mesh->vertices[i].color = color;
    }

    // Generate indices for the base of the cone
    for (int i = 0; i < sides; ++i) {
        zest_uint current_base_vertex = base_index + 1 + i;
        zest_uint next_base_vertex = base_index + 1 + ((i + 1) % sides);
        zest_PushMeshTriangle(mesh, base_index, current_base_vertex, next_base_vertex);
	}

    zest_CalculateNormals(mesh);

    return mesh;
}

zest_mesh zest_CreateSphere(zest_context context, int rings, int sectors, float radius, zest_color_t color) {
    // Calculate the angles between rings and sectors
    float ring_angle_increment = ZEST_PI / rings;
    float sector_angle_increment = 2.0f * ZEST_PI / sectors;
    float ring_angle;
    float sector_angle;

    zest_mesh mesh = zest_NewMesh(context);

    // Generate vertices for the sphere
    for (int i = 0; i <= rings; ++i)
    {
        ring_angle = ZEST_PI / 2.f - i * ring_angle_increment; /* Starting -pi/2 to pi/2 */
        float xy = radius * cosf(ring_angle);    /* r * cos(phi) */
        float z = radius * sinf(ring_angle);     /* r * sin(phi )*/

        /*
         * We add (rings + 1) vertices per longitude because of equator,
         * the North pole and South pole are not counted here, as they overlap.
         * The first and last vertices have same position and normal, but
         * different tex coords.
         */
        for (int j = 0; j <= sectors; ++j)
        {
            sector_angle = j * sector_angle_increment;
            zest_vec3 vertex;
            vertex.x = xy * cosf(sector_angle);       /* x = r * cos(phi) * cos(theta)  */
            vertex.y = xy * sinf(sector_angle);       /* y = r * cos(phi) * sin(theta) */
            vertex.z = z;                               /* z = r * sin(phi) */
            zest_PushMeshVertex(mesh, vertex.x, vertex.y, vertex.z, color);
        }
    }

    // Generate indices for the sphere    
    unsigned int k1, k2;
    for (int i = 0; i < rings; ++i)
    {
        k1 = i * (sectors + 1);
        k2 = k1 + sectors + 1;
        // 2 Triangles per latitude block excluding the first and last sectors blocks
        for (int j = 0; j < sectors; ++j, ++k1, ++k2)
        {
            if (i != 0)
            {
                zest_PushMeshIndex(mesh, k1);
                zest_PushMeshIndex(mesh, k2);
                zest_PushMeshIndex(mesh, k1 + 1);
            }

            if (i != (rings - 1))
            {
                zest_PushMeshIndex(mesh, k1 + 1);
                zest_PushMeshIndex(mesh, k2);
                zest_PushMeshIndex(mesh, k2 + 1);
            }
        }
    }

    zest_CalculateNormals(mesh);
    
    return mesh;
}

zest_mesh zest_CreateCube(zest_context context, float size, zest_color_t color) {
    zest_mesh mesh = zest_NewMesh(context);
    float half_size = size * .5f;

    // Front face
    zest_PushMeshVertex(mesh, -half_size, -half_size, -half_size, color); // 0
    zest_PushMeshVertex(mesh,  half_size, -half_size, -half_size, color); // 1
    zest_PushMeshVertex(mesh,  half_size,  half_size, -half_size, color); // 2
    zest_PushMeshVertex(mesh, -half_size,  half_size, -half_size, color); // 3
    zest_PushMeshTriangle(mesh, 2, 1, 0);
    zest_PushMeshTriangle(mesh, 0, 3, 2);

    // Back face
    zest_PushMeshVertex(mesh, -half_size, -half_size,  half_size, color); // 4
    zest_PushMeshVertex(mesh, -half_size,  half_size,  half_size, color); // 5
    zest_PushMeshVertex(mesh,  half_size,  half_size,  half_size, color); // 6
    zest_PushMeshVertex(mesh,  half_size, -half_size,  half_size, color); // 7
    zest_PushMeshTriangle(mesh, 6, 5, 4);
    zest_PushMeshTriangle(mesh, 4, 7, 6);

    // Left face
    zest_PushMeshVertex(mesh, -half_size, -half_size, -half_size, color); // 8
    zest_PushMeshVertex(mesh, -half_size,  half_size, -half_size, color); // 9
    zest_PushMeshVertex(mesh, -half_size,  half_size,  half_size, color); // 10
    zest_PushMeshVertex(mesh, -half_size, -half_size,  half_size, color); // 11
    zest_PushMeshTriangle(mesh, 10, 9, 8);
    zest_PushMeshTriangle(mesh, 8, 11, 10);

    // Right face
    zest_PushMeshVertex(mesh,  half_size, -half_size, -half_size, color); // 12
    zest_PushMeshVertex(mesh,  half_size, -half_size,  half_size, color); // 13
    zest_PushMeshVertex(mesh,  half_size,  half_size,  half_size, color); // 14
    zest_PushMeshVertex(mesh,  half_size,  half_size, -half_size, color); // 15
    zest_PushMeshTriangle(mesh, 14, 13, 12);
    zest_PushMeshTriangle(mesh, 12, 15, 14);

    // Bottom face
    zest_PushMeshVertex(mesh, -half_size, -half_size, -half_size, color); // 16
    zest_PushMeshVertex(mesh, -half_size, -half_size,  half_size, color); // 17
    zest_PushMeshVertex(mesh,  half_size, -half_size,  half_size, color); // 18
    zest_PushMeshVertex(mesh,  half_size, -half_size, -half_size, color); // 19
    zest_PushMeshTriangle(mesh, 18, 17, 16);
    zest_PushMeshTriangle(mesh, 16, 19, 18);

    // Top face
    zest_PushMeshVertex(mesh, -half_size,  half_size, -half_size, color); // 20
    zest_PushMeshVertex(mesh,  half_size,  half_size, -half_size, color); // 21
    zest_PushMeshVertex(mesh,  half_size,  half_size,  half_size, color); // 22
    zest_PushMeshVertex(mesh, -half_size,  half_size,  half_size, color); // 23
    zest_PushMeshTriangle(mesh, 22, 21, 20);
    zest_PushMeshTriangle(mesh, 20, 23, 22);

    zest_CalculateNormals(mesh);

    return mesh;
}

zest_mesh zest_CreateRoundedRectangle(zest_context context, float width, float height, float radius, int segments, zest_bool backface, zest_color_t color) {
    // Calculate the number of vertices and indices needed
    int num_vertices = segments * 4 + 8;

    zest_mesh mesh = zest_NewMesh(context);
    //zest_vec_resize(mesh.vertices, (zest_uint)num_vertices);

    // Calculate the step angle
    float angle_increment = ZEST_PI / 2.0f / segments;

    width = ZEST__MAX(radius, width - radius * 2.f);
    height = ZEST__MAX(radius, height - radius * 2.f);

    //centre vertex;
	zest_PushMeshVertex(mesh, 0.f, 0.f, 0.0f, color);

    // Bottom left corner
    for (int i = 0; i <= segments; ++i) {
        float angle = angle_increment * i;
        float x = cosf(angle) * radius;
        float y = sinf(angle) * radius;
        zest_PushMeshVertex(mesh, -width / 2.0f - x, -height / 2.0f - y, 0.0f, color);
    }

    // Bottom right corner
    for (int i = segments; i >= 0; --i) {
        float angle = angle_increment * i;
        float x = cosf(angle) * radius;
        float y = sinf(angle) * radius;
        zest_PushMeshVertex(mesh, width / 2.0f + x, -height / 2.0f - y, 0.0f, color);
    }

    // Top right corner
    for (int i = 0; i <= segments; ++i) {
        float angle = angle_increment * i;
        float x = cosf(angle) * radius;
        float y = sinf(angle) * radius;
        zest_PushMeshVertex(mesh, width / 2.0f + x, height / 2.0f + y, 0.0f, color);
    }

    // Top left corner
    for (int i = segments; i >= 0; --i) {
        float angle = angle_increment * i;
        float x = cosf(angle) * radius;
        float y = sinf(angle) * radius;
        zest_PushMeshVertex(mesh, - width / 2.0f - x, height / 2.0f + y, 0.0f, color);
    }

    zest_uint vertex_count = zest_vec_size(mesh->vertices);

    for (zest_uint i = 1; i < vertex_count - 1; ++i) {
        zest_PushMeshTriangle(mesh, 0, i, i + 1);
    }
	zest_PushMeshTriangle(mesh, 0, vertex_count - 1, 1);

    if (backface) {
		for (zest_uint i = 1; i < vertex_count - 1; ++i) {
			zest_PushMeshTriangle(mesh, 0, i + 1, i);
		}
		zest_PushMeshTriangle(mesh, 0, 1, vertex_count - 1);
    }

    return mesh;
}
//--End Instance Draw mesh layers

//Compute shaders
zest_compute zest__new_compute(zest_context context, const char* name) {
    zest_compute_handle handle = ZEST_STRUCT_LITERAL(zest_compute_handle, zest__add_store_resource(zest_handle_type_compute_pipelines, context) );
	handle.context = context;
    zest_compute compute = (zest_compute)zest__get_store_resource_checked(context, handle.value);
    *compute = ZEST__ZERO_INIT(zest_compute_t);
	handle.context = context;
    compute->magic = zest_INIT_MAGIC(zest_struct_type_compute);
    compute->backend = (zest_compute_backend)context->device->platform->new_compute_backend(context);
    compute->handle = handle;
    return compute;
}

zest_compute_builder_t zest_BeginComputeBuilder(zest_context context) {
    zest_compute_builder_t builder = ZEST__ZERO_INIT(zest_compute_builder_t);
	builder.context = context;
	//Declare the global bindings as default
	zest_SetComputeBindlessLayout(&builder, context->device->global_bindless_set_layout_handle);
    return builder;
}

void zest_SetComputeBindlessLayout(zest_compute_builder_t *builder, zest_set_layout_handle bindless_layout) {
    builder->bindless_layout = bindless_layout;
}

void zest_AddComputeSetLayout(zest_compute_builder_t *builder, zest_set_layout_handle layout_handle) {
    zest_vec_push(builder->context->device->allocator, builder->non_bindless_layouts, layout_handle);
}

zest_index zest_AddComputeShader(zest_compute_builder_t* builder, zest_shader_handle shader_handle) {
	ZEST_ASSERT(builder->context == shader_handle.context);	//The shader context must match the compute builder context!
	zest_context context = builder->context;
	zest_shader shader = (zest_shader)zest__get_store_resource_checked(shader_handle.context, shader_handle.value);
    zest_vec_push(builder->context->device->allocator, builder->shaders, shader_handle);
    return zest_vec_last_index(builder->shaders);
}

void zest_SetComputePushConstantSize(zest_compute_builder_t* builder, zest_uint size) {
    assert(size && size <= 256);        //push constants must be within 0 and 256 bytes in size
    builder->push_constant_size = size;
}

void zest_SetComputeUserData(zest_compute_builder_t* builder, void* data) {
    builder->user_data = data;
}

void zest_SetComputeManualRecord(zest_compute_builder_t *builder) {
    ZEST__FLAG(builder->flags, zest_compute_flag_manual_fif);
}

void zest_SetComputePrimaryRecorder(zest_compute_builder_t *builder) {
    ZEST__FLAG(builder->flags, zest_compute_flag_primary_recorder);
}

zest_compute_handle zest_FinishCompute(zest_compute_builder_t *builder, const char *name) {
	zest_context context = builder->context;
	zest_compute compute = zest__new_compute(context, name);
    compute->user_data = builder->user_data;
    compute->flags = builder->flags;

    compute->bindless_layout = builder->bindless_layout;

    ZEST__FLAG(compute->flags, zest_compute_flag_sync_required);
    ZEST__FLAG(compute->flags, zest_compute_flag_rewrite_command_buffer);
    ZEST__MAYBE_FLAG(compute->flags, zest_compute_flag_manual_fif, builder->flags & zest_compute_flag_manual_fif);

    ZEST__FLAG(compute->flags, zest_compute_flag_is_active);

    zest_bool result = context->device->platform->finish_compute(builder, compute);
    zest_vec_free(builder->context->device->allocator, builder->non_bindless_layouts);
    zest_vec_free(builder->context->device->allocator, builder->shaders);
    if (!result) {
        zest__cleanup_compute(compute);
        return ZEST__ZERO_INIT(zest_compute_handle);
    }
    return compute->handle;
}

void zest__cleanup_compute(zest_compute compute) {
	zest_context context = compute->handle.context;
    context->device->platform->cleanup_compute_backend(compute);
    zest__remove_store_resource(compute->handle.context, compute->handle.value);
}
//--End Compute shaders

//-- Start debug helpers
void zest_OutputMemoryUsage(zest_context context) {
    printf("Host Memory Pools\n");
    zest_size total_host_memory = 0;
    zest_size total_device_memory = 0;
    for (int i = 0; i != context->device->memory_pool_count; ++i) {
        printf("\tMemory Pool Size: %zu\n", context->device->memory_pool_sizes[i]);
        total_host_memory += context->device->memory_pool_sizes[i];
    }
    printf("Device Memory Pools\n");
    zest_map_foreach(i, context->device->buffer_allocators) {
        zest_buffer_allocator buffer_allocator = *zest_map_at_index(context->device->buffer_allocators, i);
        zest_buffer_usage_t usage = { buffer_allocator->buffer_info.buffer_usage_flags, buffer_allocator->buffer_info.property_flags, buffer_allocator->buffer_info.image_usage_flags };
        zest_key usage_key = zest_map_hash_ptr(context->device->pool_sizes, &usage, sizeof(zest_buffer_usage_t));
        zest_buffer_pool_size_t pool_size = *zest_map_at_key(context->device->pool_sizes, usage_key);
        if (buffer_allocator->buffer_info.image_usage_flags) {
            printf("\t%s (%s), Usage: %u, Image Usage: %u\n", "Image", pool_size.name, buffer_allocator->buffer_info.buffer_usage_flags, buffer_allocator->buffer_info.image_usage_flags);
        }
        else {
            printf("\t%s (%s), Usage: %u, Properties: %u\n", "Buffer", pool_size.name, buffer_allocator->buffer_info.buffer_usage_flags, buffer_allocator->buffer_info.property_flags);
        }
        zest_vec_foreach(j, buffer_allocator->memory_pools) {
            zest_device_memory_pool memory_pool = buffer_allocator->memory_pools[j];
            printf("\t\tMemory Pool Size: %llu\n", memory_pool->size);
            total_device_memory += memory_pool->size;
        }
    }
    printf("Total Host Memory: %zu (%llu MB)\n", total_host_memory, total_host_memory / zloc__MEGABYTE(1));
    printf("Total Device Memory: %zu (%llu MB)\n", total_device_memory, total_device_memory / zloc__MEGABYTE(1));
    printf("\n");
    printf("\t\t\t\t-- * --\n");
    printf("\n");
}

void zest_PrintReports(zest_context context) {
    if (zest_map_size(context->device->reports)) {
		ZEST_PRINT("--- Zest Reports ---");
        ZEST_PRINT("");
        zest_map_foreach(i, context->device->reports) {
            zest_report_t *report = &context->device->reports.data[i];
			ZEST_PRINT("Count: %i. Message in file [%s] function [%s] on line %i: %s", report->count, report->file_name, report->function_name, report->line_number, report->message.str);
            ZEST_PRINT("-------------------------------------------------");
        }
        ZEST_PRINT("");
    }
}

const char *zest__struct_type_to_string(zest_struct_type struct_type) {
    switch (struct_type) {
	case zest_struct_type_view                    : return "image_view"; break;
	case zest_struct_type_view_array              : return "image_view_array"; break;
	case zest_struct_type_image                   : return "image"; break;
	case zest_struct_type_imgui_image             : return "imgui_image"; break;
	case zest_struct_type_image_collection        : return "image_collection"; break;
	case zest_struct_type_sampler                 : return "sampler"; break;
	case zest_struct_type_layer                   : return "layer"; break;
	case zest_struct_type_pipeline                : return "pipeline"; break;
	case zest_struct_type_pipeline_template       : return "pipeline_template"; break;
	case zest_struct_type_set_layout              : return "set_layout"; break;
	case zest_struct_type_descriptor_set          : return "descriptor_set"; break;
	case zest_struct_type_shader_resources        : return "shader_resources"; break;
	case zest_struct_type_uniform_buffer          : return "uniform_buffer"; break;
	case zest_struct_type_buffer_allocator        : return "buffer_allocator"; break;
	case zest_struct_type_descriptor_pool         : return "descriptor_pool"; break;
	case zest_struct_type_compute                 : return "compute"; break;
	case zest_struct_type_buffer                  : return "buffer"; break;
	case zest_struct_type_device_memory_pool      : return "device_memory_pool"; break;
	case zest_struct_type_timer                   : return "timer"; break;
	case zest_struct_type_window                  : return "window"; break;
	case zest_struct_type_shader                  : return "shader"; break;
	case zest_struct_type_imgui                   : return "imgui"; break;
	case zest_struct_type_queue                   : return "queue"; break;
	case zest_struct_type_execution_timeline      : return "execution_timeline"; break;
	case zest_struct_type_frame_graph_semaphores  : return "frame_graph_semaphores"; break;
	case zest_struct_type_swapchain               : return "swapchain"; break;
	case zest_struct_type_frame_graph             : return "frame_graph"; break;
	case zest_struct_type_pass_node               : return "pass_node"; break;
	case zest_struct_type_resource_node           : return "resource_node"; break;
	case zest_struct_type_wave_submission         : return "wave_submission"; break;
	case zest_struct_type_device                  : return "device"; break;
	case zest_struct_type_app                     : return "app"; break;
	case zest_struct_type_vector                  : return "vector"; break;
	case zest_struct_type_bitmap                  : return "bitmap"; break;
	case zest_struct_type_render_target_group     : return "render_target_group"; break;
	case zest_struct_type_slang_info              : return "slang_info"; break;
	case zest_struct_type_render_pass             : return "render_pass"; break;
	case zest_struct_type_mesh                    : return "mesh"; break;
	case zest_struct_type_texture_asset           : return "texture_asset"; break;
	case zest_struct_type_atlas_region            : return "atlas_region"; break;
    default: return "UNKNOWN"; break;
    }
    return "UNKNOWN";
}

const char *zest__platform_command_to_string(zest_platform_command command) {
    switch (command) {
		case zest_command_surface                : return "Surface"; break;
		case zest_command_instance               : return "Instance"; break;
		case zest_command_logical_device         : return "Instance"; break;
		case zest_command_debug_messenger        : return "Debug Messenger"; break;
		case zest_command_device_instance        : return "Device Instance"; break;
		case zest_command_semaphore              : return "Semaphore"; break;
		case zest_command_command_pool           : return "Command Pool"; break;
		case zest_command_command_buffer         : return "Command Buffer"; break;
		case zest_command_buffer                 : return "Buffer"; break;
		case zest_command_allocate_memory_pool   : return "Allocate Memory Pool"; break;
		case zest_command_allocate_memory_image  : return "Allocate Memory Image"; break;
		case zest_command_fence                  : return "Fence"; break;
		case zest_command_swapchain              : return "Swapchain"; break;
		case zest_command_pipeline_cache         : return "Pipeline Cache"; break;
		case zest_command_descriptor_layout      : return "Descriptor Layout"; break;
		case zest_command_descriptor_pool        : return "Descriptor Pool"; break;
		case zest_command_pipeline_layout        : return "Pipeline Layout"; break;
		case zest_command_pipelines              : return "Pipelines"; break;
		case zest_command_shader_module          : return "Shader Module"; break;
		case zest_command_sampler                : return "Sampler"; break;
		case zest_command_image                  : return "Image"; break;
		case zest_command_image_view             : return "Image View"; break;
		case zest_command_render_pass            : return "Render Pass"; break;
		case zest_command_frame_buffer           : return "Frame Buffer"; break;
		case zest_command_query_pool             : return "Query Pool"; break;
		case zest_command_compute_pipeline       : return "Compute Pipeline"; break;
		default: return "UNKNOWN"; break;
    }
    return "UNKNOWN";
}

const char *zest__platform_context_to_string(zest_platform_memory_context context) {
    switch (context) {
		case zest_platform_context         : return "Renderer"; break;
		case zest_platform_device           : return "Device"; break;
		default: return "UNKNOWN"; break;
    }
    return "UNKNOWN";
}

void zest__print_block_info(zest_context context, void *allocation, zloc_header *current_block, zest_platform_memory_context context_filter, zest_platform_command command_filter) {
    if (ZEST_VALID_HANDLE(allocation)) {
		zest_struct_type struct_type = (zest_struct_type)ZEST_STRUCT_TYPE(allocation);
		ptrdiff_t offset_from_allocator = (ptrdiff_t)allocation - (ptrdiff_t)context->device->allocator;
		zest_vec *vector = (zest_vec*)allocation;
		switch (struct_type) {
		case zest_struct_type_vector:
			ZEST_PRINT("Allocation: %p, id: %i, size: %zu, offset: %zu, type: %s", allocation, vector->id, current_block->size, offset_from_allocator, zest__struct_type_to_string(struct_type));
            break;
		default:
			ZEST_PRINT("Allocation: %p, size: %zu, offset: %zu, type: %s", allocation, current_block->size, offset_from_allocator, zest__struct_type_to_string(struct_type));
			break;
		}
    } else {
        //Is it a vulkan allocation?
        zest_platform_memory_info_t *vulkan_info = (zest_platform_memory_info_t *)allocation;
        zest_uint mem_context = (vulkan_info->context_info & 0xff0000) >> 16;
        zest_uint command = (vulkan_info->context_info & 0xff000000) >> 24;
        if ((!command_filter || command == command_filter) && (!context_filter || context_filter == mem_context)) {
            if (ZEST_IS_INTITIALISED(vulkan_info->context_info)) {
                ZEST_PRINT("Vulkan allocation: %p, Timestamp: %u, Category: %s, Command: %s", allocation, vulkan_info->timestamp,
					zest__platform_context_to_string((zest_platform_memory_context)mem_context),
					zest__platform_command_to_string((zest_platform_command)command)
                );
			} else if(zloc__block_size(current_block) > 0) {
				//Unknown block, print what we can about it
                ptrdiff_t offset_from_allocator = (ptrdiff_t)allocation - (ptrdiff_t)context->device->allocator;
				ZEST_PRINT("Unknown Block - size: %zu (%p) Offset from allocator: %zu", zloc__block_size(current_block), allocation, offset_from_allocator);
            }
        }
    }
}

void zest_PrintMemoryBlocks(zest_context context, zloc_header *first_block, zest_bool output_all, zest_platform_memory_context context_filter, zest_platform_command command_filter) {
    zloc_pool_stats_t stats = ZEST__ZERO_INIT(zloc_pool_stats_t);
    zest_memory_stats_t zest_stats = ZEST__ZERO_INIT(zest_memory_stats_t);
    zloc_header *current_block = first_block;
    ZEST_PRINT("Current used blocks in ZEST memory");
    while (!zloc__is_last_block_in_pool(current_block)) {
        if (zloc__is_free_block(current_block)) {
            stats.free_blocks++;
            stats.free_size += zloc__block_size(current_block);
        } else {
            stats.used_blocks++;
            zest_size block_size = zloc__block_size(current_block);
            stats.used_size += block_size;
            void *allocation = (void*)((char*)current_block + zloc__BLOCK_POINTER_OFFSET);
            if (output_all) {
                zest__print_block_info(context, allocation, current_block, context_filter, command_filter);
            }
			zest_platform_memory_info_t *vulkan_info = (zest_platform_memory_info_t *)allocation;
			zest_uint mem_context = (vulkan_info->context_info & 0xff0000) >> 16;
			zest_uint command = (vulkan_info->context_info & 0xff000000) >> 24;
            if (ZEST_IS_INTITIALISED(vulkan_info->context_info)) {
                if (mem_context == zest_platform_device) {
                    zest_stats.device_allocations++;
                } else if (mem_context == zest_platform_context) {
                    zest_stats.renderer_allocations++;
                }
                if (command_filter == command && context_filter == mem_context) {
                    zest_stats.filter_count++;
                }
            }
        }
        current_block = zloc__next_physical_block(current_block);
    }
    if (zloc__is_free_block(current_block)) {
        stats.free_blocks++;
        stats.free_size += zloc__block_size(current_block);
    } else {
		zest_size block_size = zloc__block_size(current_block);
		void *allocation = (void*)((char*)current_block + zloc__BLOCK_SIZE_OVERHEAD);
        if (block_size) {
			stats.used_blocks++;
            stats.used_size += block_size ? block_size : 0;
			ZEST_PRINT("Block size: %zu (%p)", zloc__block_size(current_block), allocation);
        }
        if (output_all) {
            zest__print_block_info(context, allocation, current_block, context_filter, command_filter);
        }
        zest_platform_memory_info_t *vulkan_info = (zest_platform_memory_info_t *)allocation;
        zest_uint mem_context = (vulkan_info->context_info & 0xff0000) >> 16;
        zest_uint command = (vulkan_info->context_info & 0xff000000) >> 24;
        if (ZEST_IS_INTITIALISED(vulkan_info->context_info)) {
            if (mem_context == zest_platform_device) {
                zest_stats.device_allocations++;
            } else if (mem_context == zest_platform_context) {
                zest_stats.renderer_allocations++;
            }
            if (command_filter == command && context_filter == mem_context) {
                zest_stats.filter_count++;
            }
        }
    }
    ZEST_PRINT("Free blocks: %u, Used blocks: %u", stats.free_blocks, stats.used_blocks);
    ZEST_PRINT("Device allocations: %u, Renderer Allocations: %u, Filter Allocations: %u", zest_stats.device_allocations, zest_stats.renderer_allocations, zest_stats.filter_count);
}

zest_uint zest_GetValidationErrorCount(zest_context context) {
    return zest_map_size(context->device->validation_errors);
}

void zest_ResetValidationErrors(zest_context context) {
    zest_map_foreach(i, context->device->validation_errors) {
        zest_FreeText(context->device->allocator, &context->device->validation_errors.data[i]);
    }
    zest_map_free(context->device->allocator, context->device->validation_errors);
}

zest_bool zest_SetErrorLogPath(zest_device device, const char* path) {
    ZEST_ASSERT_HANDLE(device);    //Have you initialised Zest yet?
    zest_SetTextf(device->allocator, &device->log_path, "%s/%s", path, "zest_log.txt");
    FILE* log_file = zest__open_file(device->log_path.str, "wb");
    if (!log_file) {
        printf("Could Not Create Log file!");
        return ZEST_FALSE;
    }
    ZEST_LOG(log_file, "Start of Log\n");
    fclose(log_file);
    return ZEST_TRUE;
}
//-- End Debug helpers

//-- Timer functions
zest_timer_handle zest_CreateTimer(zest_context context, double update_frequency) {
    zest_timer_handle handle = { zest__add_store_resource(zest_handle_type_timers, context) };
	handle.context = context;
    zest_timer timer = (zest_timer)zest__get_store_resource_checked(context, handle.value);
    *timer = ZEST__ZERO_INIT(zest_timer_t);
    timer->magic = zest_INIT_MAGIC(zest_struct_type_timer);
    timer->handle = handle;
    timer->update_frequency = update_frequency;
    timer->update_tick_length = ZEST_MICROSECS_SECOND / timer->update_frequency;
    timer->update_time = 1.f / timer->update_frequency;
    timer->ticker = zest_Microsecs() - timer->update_tick_length;
    timer->max_elapsed_time = timer->update_tick_length * 4.0;
    timer->start_time = (double)zest_Microsecs();
    timer->current_time = (double)zest_Microsecs();
    return handle;
}

void zest_FreeTimer(zest_timer_handle timer_handle) {
    zest_timer timer = (zest_timer)zest__get_store_resource_checked(timer_handle.context, timer_handle.value);
    zest__remove_store_resource(timer_handle.context, timer_handle.value);
}

void zest_TimerReset(zest_timer_handle timer_handle) {
    zest_timer timer = (zest_timer)zest__get_store_resource_checked(timer_handle.context, timer_handle.value);
    timer->start_time = (double)zest_Microsecs();
    timer->current_time = (double)zest_Microsecs();
}

double zest_TimerDeltaTime(zest_timer_handle timer_handle) {
    zest_timer timer = (zest_timer)zest__get_store_resource_checked(timer_handle.context, timer_handle.value);
    return timer->delta_time;
}

void zest_TimerTick(zest_timer_handle timer_handle) {
    zest_timer timer = (zest_timer)zest__get_store_resource_checked(timer_handle.context, timer_handle.value);
    timer->delta_time = (double)zest_Microsecs() - timer->start_time;
}

double zest_TimerUpdateTime(zest_timer_handle timer_handle) {
    zest_timer timer = (zest_timer)zest__get_store_resource_checked(timer_handle.context, timer_handle.value);
    return timer->update_time;
}

double zest_TimerFrameLength(zest_timer_handle timer_handle) {
    zest_timer timer = (zest_timer)zest__get_store_resource_checked(timer_handle.context, timer_handle.value);
    return timer->update_tick_length;
}

double zest_TimerAccumulate(zest_timer_handle timer_handle) {
    zest_timer timer = (zest_timer)zest__get_store_resource_checked(timer_handle.context, timer_handle.value);
    double new_time = (double)zest_Microsecs();
    double frame_time = fabs(new_time - timer->current_time);
    frame_time = ZEST__MIN(frame_time, ZEST_MICROSECS_SECOND);
    timer->current_time = new_time;

    timer->accumulator_delta = timer->accumulator;
    timer->accumulator += frame_time;
    timer->accumulator = ZEST__MIN(timer->accumulator, timer->max_elapsed_time);
    timer->update_count = 0;
    return frame_time;
}

int zest_TimerPendingTicks(zest_timer_handle timer_handle) {
    zest_timer timer = (zest_timer)zest__get_store_resource_checked(timer_handle.context, timer_handle.value);
    return (int)(timer->accumulator / timer->update_tick_length);
}

void zest_TimerUnAccumulate(zest_timer_handle timer_handle) {
    zest_timer timer = (zest_timer)zest__get_store_resource_checked(timer_handle.context, timer_handle.value);
    timer->accumulator -= timer->update_tick_length;
    timer->accumulator_delta = timer->accumulator_delta - timer->accumulator;
    timer->update_count++;
    timer->time_passed += timer->update_time;
    timer->seconds_passed += timer->update_time * 1000.f;
}

zest_bool zest_TimerDoUpdate(zest_timer_handle timer_handle) {
    zest_timer timer = (zest_timer)zest__get_store_resource_checked(timer_handle.context, timer_handle.value);
    return timer->accumulator >= timer->update_tick_length;
}

double zest_TimerLerp(zest_timer_handle timer_handle) {
    zest_timer timer = (zest_timer)zest__get_store_resource_checked(timer_handle.context, timer_handle.value);
    return timer->update_count > 1 ? 1.f : timer->lerp;
}

void zest_TimerSet(zest_timer_handle timer_handle) {
    zest_timer timer = (zest_timer)zest__get_store_resource_checked(timer_handle.context, timer_handle.value);
    timer->lerp = timer->accumulator / timer->update_tick_length;
}

void zest_TimerSetMaxFrames(zest_timer_handle timer_handle, double frames) {
    zest_timer timer = (zest_timer)zest__get_store_resource_checked(timer_handle.context, timer_handle.value);
    timer->max_elapsed_time = timer->update_tick_length * frames;
}

void zest_TimerSetUpdateFrequency(zest_timer_handle timer_handle, double frequency) {
    zest_timer timer = (zest_timer)zest__get_store_resource_checked(timer_handle.context, timer_handle.value);
    timer->update_frequency = frequency;
    timer->update_tick_length = ZEST_MICROSECS_SECOND / timer->update_frequency;
    timer->update_time = 1.f / timer->update_frequency;
    timer->ticker = zest_Microsecs() - timer->update_tick_length;
}

double zest_TimerUpdateFrequency(zest_timer_handle timer_handle) {
    zest_timer timer = (zest_timer)zest__get_store_resource_checked(timer_handle.context, timer_handle.value);
    return timer->update_frequency;
}

zest_bool zest_TimerUpdateWasRun(zest_timer_handle timer_handle) {
    zest_timer timer = (zest_timer)zest__get_store_resource_checked(timer_handle.context, timer_handle.value);
    return timer->update_count > 0;
}
//-- End Timer Functions

//-- Command_buffer_functions
// -- Frame_graph_context_functions
zest_bool zest_cmd_UploadBuffer(const zest_command_list command_list, zest_buffer_uploader_t *uploader) {
    if (!zest_vec_size(uploader->buffer_copies)) {
        return ZEST_FALSE;
    }
    return command_list->context->device->platform->upload_buffer(command_list, uploader);
}

void zest_cmd_DrawIndexed(const zest_command_list command_list, zest_uint index_count, zest_uint instance_count, zest_uint first_index, int32_t vertex_offset, zest_uint first_instance) {
    ZEST_ASSERT_HANDLE(command_list);        //Not valid command_list, this command must be called within a frame graph execution callback
	command_list->context->device->platform->draw_indexed(command_list, index_count, instance_count, first_index, vertex_offset, first_instance);
}

void zest_cmd_CopyBuffer(const zest_command_list command_list, zest_buffer src_buffer, zest_buffer dst_buffer, zest_size size) {
    ZEST_ASSERT(size <= src_buffer->size);        //size must be less than or equal to the staging buffer size and the device buffer size
    ZEST_ASSERT(size <= dst_buffer->size);
    ZEST_ASSERT_HANDLE(command_list);                  //Not valid command_list, this command must be called within a frame graph execution callback
	command_list->context->device->platform->copy_buffer(command_list, src_buffer, dst_buffer, size);
}

void zest_cmd_BindPipelineShaderResource(const zest_command_list command_list, zest_pipeline pipeline, zest_shader_resources_handle handle) {
    ZEST_ASSERT_HANDLE(command_list);        //Not valid command_list, this command must be called within a frame graph execution callback
	command_list->context->device->platform->bind_pipeline_shader_resource(command_list, pipeline, handle);
}

void zest_cmd_BindPipeline(const zest_command_list command_list, zest_pipeline pipeline, zest_descriptor_set *descriptor_sets, zest_uint set_count) {
    ZEST_ASSERT_HANDLE(command_list);        //Not valid command_list, this command must be called within a frame graph execution callback
    ZEST_ASSERT(set_count && descriptor_sets);    //No descriptor sets. Must bind the pipeline with a valid desriptor set
	command_list->context->device->platform->bind_pipeline(command_list, pipeline, descriptor_sets, set_count);
}

void zest_cmd_BindComputePipeline(const zest_command_list command_list, zest_compute_handle compute_handle, zest_descriptor_set *descriptor_sets, zest_uint set_count) {
    ZEST_ASSERT_HANDLE(command_list);        //Not valid command_list, this command must be called within a frame graph execution callback
    ZEST_ASSERT(set_count && descriptor_sets);   //No descriptor sets. Must bind the pipeline with a valid desriptor set
	command_list->context->device->platform->bind_compute_pipeline(command_list, compute_handle, descriptor_sets, set_count);
}

void zest_cmd_BindVertexBuffer(const zest_command_list command_list, zest_uint first_binding, zest_uint binding_count, zest_buffer buffer) {
    ZEST_ASSERT_HANDLE(command_list);        //Not valid command_list, this command must be called within a frame graph execution callback
	command_list->context->device->platform->bind_vertex_buffer(command_list, first_binding, binding_count, buffer);
}

void zest_cmd_BindIndexBuffer(const zest_command_list command_list, zest_buffer buffer) {
    ZEST_ASSERT_HANDLE(command_list);        //Not valid command_list, this command must be called within a frame graph execution callback
	command_list->context->device->platform->bind_index_buffer(command_list, buffer);
}

void zest_cmd_SendPushConstants(const zest_command_list command_list, zest_pipeline pipeline, void *data) {
    ZEST_ASSERT_HANDLE(command_list);        //Not valid command_list, this command must be called within a frame graph execution callback
	command_list->context->device->platform->send_push_constants(command_list, pipeline, data);
}

void zest_cmd_SendCustomComputePushConstants(const zest_command_list command_list, zest_compute_handle compute_handle, const void *push_constant) {
    ZEST_ASSERT_HANDLE(command_list);        //Not valid command_list, this command must be called within a frame graph execution callback
	command_list->context->device->platform->send_custom_compute_push_constants(command_list, compute_handle, push_constant);
}

void zest_cmd_Draw(const zest_command_list command_list, zest_uint vertex_count, zest_uint instance_count, zest_uint first_vertex, zest_uint first_instance) {
    ZEST_ASSERT_HANDLE(command_list);        //Not valid command_list, this command must be called within a frame graph execution callback
	command_list->context->device->platform->draw(command_list, vertex_count, instance_count, first_vertex, first_instance);
}

void zest_cmd_DrawLayerInstruction(const zest_command_list command_list, zest_uint vertex_count, zest_layer_instruction_t *instruction) {
    ZEST_ASSERT_HANDLE(command_list);        //Not valid command_list, this command must be called within a frame graph execution callback
	command_list->context->device->platform->draw_layer_instruction(command_list, vertex_count, instruction);
}

void zest_cmd_DispatchCompute(const zest_command_list command_list, zest_compute_handle compute_handle, zest_uint group_count_x, zest_uint group_count_y, zest_uint group_count_z) {
    ZEST_ASSERT_HANDLE(command_list);        //Not valid command_list, this command must be called within a frame graph execution callback
	command_list->context->device->platform->dispatch_compute(command_list, compute_handle, group_count_x, group_count_y, group_count_z);
}

void zest_cmd_SetScreenSizedViewport(const zest_command_list command_list, float min_depth, float max_depth) {
    //This function must be called within a frame graph execution pass callback
    ZEST_ASSERT(command_list);    //Must be a valid command buffer
    ZEST_ASSERT_HANDLE(command_list->frame_graph);
    ZEST_ASSERT_HANDLE(command_list->frame_graph->swapchain);    //frame graph must be set up with a swapchain to use this function
	command_list->context->device->platform->set_screensized_viewport(command_list, min_depth, max_depth);
}

void zest_cmd_Scissor(const zest_command_list command_list, zest_scissor_rect_t *scissor) {
    ZEST_ASSERT_HANDLE(command_list);        //Not valid command_list, this command must be called within a frame graph execution callback
	command_list->context->device->platform->scissor(command_list, scissor);
}

void zest_cmd_ViewPort(const zest_command_list command_list, zest_viewport_t *viewport) {
    ZEST_ASSERT_HANDLE(command_list);        //Not valid command_list, this command must be called within a frame graph execution callback
	command_list->context->device->platform->viewport(command_list, viewport);
}

void zest_cmd_BlitImageMip(const zest_command_list command_list, zest_resource_node src, zest_resource_node dst, zest_uint mip_to_blit, zest_supported_pipeline_stages pipeline_stage) {
    ZEST_ASSERT_HANDLE(command_list);        //Not valid command_list, this command must be called within a frame graph execution callback
    ZEST_ASSERT_HANDLE(src);
    ZEST_ASSERT_HANDLE(dst);
    ZEST_ASSERT(src->type == zest_resource_type_image && dst->type == zest_resource_type_image);
    //Source and destination images must be the same width/height and have the same number of mip levels
    ZEST_ASSERT(src->image.info.extent.width == dst->image.info.extent.width);
    ZEST_ASSERT(src->image.info.extent.height == dst->image.info.extent.height);
    ZEST_ASSERT(src->image.info.mip_levels == dst->image.info.mip_levels);
	command_list->context->device->platform->blit_image_mip(command_list, src, dst, mip_to_blit, pipeline_stage);
}

void zest_cmd_CopyImageMip(const zest_command_list command_list, zest_resource_node src, zest_resource_node dst, zest_uint mip_to_copy, zest_supported_pipeline_stages pipeline_stage) {
    ZEST_ASSERT_HANDLE(command_list);        //Not valid command_list, this command must be called within a frame graph execution callback
    ZEST_ASSERT_HANDLE(src);
    ZEST_ASSERT_HANDLE(dst);
    ZEST_ASSERT(src->type == zest_resource_type_image && dst->type == zest_resource_type_image);
    //Source and destination images must be the same width/height and have the same number of mip levels
    ZEST_ASSERT(src->image.info.extent.width == dst->image.info.extent.width);
    ZEST_ASSERT(src->image.info.extent.height == dst->image.info.extent.height);
    ZEST_ASSERT(src->image.info.mip_levels == dst->image.info.mip_levels);

    //You must ensure that when creating the images that you use usage hints to indicate that you intend to copy
    //the images. When creating a transient image resource you can set the usage hints in the zest_image_resource_info_t
    //type that you pass into zest_CreateTransientImageResource. Trying to copy images that don't have the appropriate
    //usage flags set will result in validation errors.
    ZEST_ASSERT(src->image.info.flags & zest_image_flag_transfer_src);
    ZEST_ASSERT(dst->image.info.flags & zest_image_flag_transfer_dst);
	command_list->context->device->platform->copy_image_mip(command_list, src, dst, mip_to_copy, pipeline_stage);
}

void zest_cmd_Clip(const zest_command_list command_list, float x, float y, float width, float height, float min_depth, float max_depth) {
    ZEST_ASSERT_HANDLE(command_list);        //Not valid command_list, this command must be called within a frame graph execution callback
	command_list->context->device->platform->clip(command_list, x, y, width, height, min_depth, max_depth);
}

void zest_cmd_InsertComputeImageBarrier(const zest_command_list command_list, zest_resource_node resource, zest_uint base_mip) {
    ZEST_ASSERT_HANDLE(command_list);        //Not valid command_list, this command must be called within a frame graph execution callback
    ZEST_ASSERT_HANDLE(resource);    //Not a valid resource handle!
    ZEST_ASSERT(resource->type == zest_resource_type_image);    //resource type must be an image
	command_list->context->device->platform->insert_compute_image_barrier(command_list, resource, base_mip);
}

zest_bool zest_cmd_ImageClear(const zest_command_list command_list, zest_image_handle handle) {
	return command_list->context->device->platform->image_clear(command_list, handle);
}

void zest_cmd_BindMeshVertexBuffer(const zest_command_list command_list, zest_layer_handle layer_handle) {
    ZEST_ASSERT_HANDLE(command_list);        //Not valid command_list, this command must be called within a frame graph execution callback
	command_list->context->device->platform->bind_mesh_vertex_buffer(command_list, layer_handle);
}

void zest_cmd_BindMeshIndexBuffer(const zest_command_list command_list, zest_layer_handle layer_handle) {
    ZEST_ASSERT_HANDLE(command_list);        //Not valid command_list, this command must be called within a frame graph execution callback
	command_list->context->device->platform->bind_mesh_index_buffer(command_list, layer_handle);
}
//-- End Command_buffer_functions

#endif