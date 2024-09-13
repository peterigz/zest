#pragma once

#include <zest.h>
#include "implementations/impl_glfw.h"
#include "implementations/impl_imgui_glfw.h"
#include "imgui/imgui.h"
#include <imgui/misc/freetype/imgui_freetype.h>
#include <imgui/backends/imgui_impl_glfw.h>
#include <random>
#include <intrin.h>

const float pi = 3.14159265359f;
const float two_pi = 2.f * 3.14159265359f;

typedef zest_uint zest_axis_flags;
enum zest_axis_bits {
	zest_Axis_none = 0,
	zest_axis_x		= 1 << 0,
	zest_axis_y		= 1 << 1,
	zest_axis_z		= 1 << 2
};

enum zest_widget_type {
	zest_widget_type_move,
	zest_widget_type_scale,
};

enum zest_widget_part_index {
	zest_x_plane,
	zest_y_plane,
	zest_z_plane,
	zest_x_rail,
	zest_y_rail,
	zest_z_rail,
	zest_part_none
};

struct Random {
	std::mt19937 gen;
	std::uniform_real_distribution<> dis;

	Random() : gen(std::random_device{}()), dis(-1.0, 1.0) {}

	double uniform() {
		return dis(gen);
	}

	int randInt(int min, int max) {
		return std::uniform_int_distribution<>(min, max)(gen);
	}
};

const uint8_t perm[] =
{
	151,160,137,91,90,15,
	131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,8,99,37,240,21,10,23,
	190, 6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,35,11,32,57,177,33,
	88,237,149,56,87,174,20,125,136,171,168, 68,175,74,165,71,134,139,48,27,166,
	77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,55,46,245,40,244,
	102,143,54, 65,25,63,161, 1,216,80,73,209,76,132,187,208, 89,18,169,200,196,
	135,130,116,188,159,86,164,100,109,198,173,186, 3,64,52,217,226,250,124,123,
	5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,189,28,42,
	223,183,170,213,119,248,152, 2,44,154,163, 70,221,153,101,155,167, 43,172,9,
	129,22,39,253, 19,98,108,110,79,113,224,232,178,185, 112,104,218,246,97,228,
	251,34,242,193,238,210,144,12,191,179,162,241, 81,51,145,235,249,14,239,107,
	49,192,214, 31,181,199,106,157,184, 84,204,176,115,121,50,45,127, 4,150,254,
	138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180,
	151,160,137,91,90,15,
	131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,8,99,37,240,21,10,23,
	190, 6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,35,11,32,57,177,33,
	88,237,149,56,87,174,20,125,136,171,168, 68,175,74,165,71,134,139,48,27,166,
	77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,55,46,245,40,244,
	102,143,54, 65,25,63,161, 1,216,80,73,209,76,132,187,208, 89,18,169,200,196,
	135,130,116,188,159,86,164,100,109,198,173,186, 3,64,52,217,226,250,124,123,
	5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,189,28,42,
	223,183,170,213,119,248,152, 2,44,154,163, 70,221,153,101,155,167, 43,172,9,
	129,22,39,253, 19,98,108,110,79,113,224,232,178,185, 112,104,218,246,97,228,
	251,34,242,193,238,210,144,12,191,179,162,241, 81,51,145,235,249,14,239,107,
	49,192,214, 31,181,199,106,157,184, 84,204,176,115,121,50,45,127, 4,150,254,
	138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180
};

float hash(zest_ivec2 p) {

	// 2D -> 1D
	int n = p.x * 3 + p.y * 113;

	// 1D hash by Hugo Elias
	n = (n << 13) ^ n;
	n = n * (n * n * 15731 + 789221) + 1376312589;
	return -1.f + 2.f * float(n & 0x0fffffff) / float(0x0fffffff);
}

uint32_t SeedGen(zest_ivec2 p)
{
	uint32_t h = (uint32_t)p.x + (uint32_t)p.y * 2654435761u;
	h ^= h >> 15;
	h *= 0x85ebca6b;
	h ^= h >> 13;
	h *= 0xc2b2ae35;
	h ^= h >> 16;
	return h;
}

uint32_t SeedGen(uint32_t h)
{
	h ^= h >> 15;
	h *= 0x85ebca6b;
	h ^= h >> 13;
	h *= 0xc2b2ae35;
	h ^= h >> 16;
	return h;
}

float xxhash32(zest_ivec2 p)
{
	uint32_t h = (uint32_t)p.x + (uint32_t)p.y * 2654435761u;
	h ^= h >> 15;
	h *= 0x85ebca6b;
	h ^= h >> 13;
	h *= 0xc2b2ae35;
	h ^= h >> 16;
	return -1.0f + 2.0f * float(h) / float(UINT32_MAX);
}

float noise(zest_vec2 p)
{
	zest_ivec2 i = { (int)p.x, (int)p.y };
	zest_vec2 f = { p.x - (int)p.x, p.y - (int)p.y };
	zest_vec2 u = { f.x * f.x * (3.f - 2.f * f.x), f.y * f.y * (3.f - 2.f * f.y) };
	return	zest_Lerp(	zest_Lerp(	xxhash32(zest_AddiVec2(i, {0, 0})),
									xxhash32(zest_AddiVec2(i, {1, 0})), u.x),
						zest_Lerp(	xxhash32(zest_AddiVec2(i, {0, 1})),
									xxhash32(zest_AddiVec2(i, {1, 1})), u.x), u.y);
}

class ValueNoise {
private:
	static constexpr int TABLE_SIZE = 256;

	float lerp(float a, float b, float t) {
		return a + t * (b - a);
	}

	float smoothstep(float t) {
		return t * t * (3 - 2 * t);
	}

public:
	float noise(float x) {
		int x0 = static_cast<int>(std::floor(x)) & (TABLE_SIZE - 1);
		int x1 = (x0 + 1) & (TABLE_SIZE - 1);
		float t = x - std::floor(x);
		float st = smoothstep(t);

		float n0 = static_cast<float>(perm[x0]) / 255.0f;
		float n1 = static_cast<float>(perm[x1]) / 255.0f;

		return lerp(n0, n1, st);
	}
};

struct ellipsoid {
	zest_vec3 position;
	zest_vec3 radius;
};

struct cylinder_t {
	zest_vec3 position;
	zest_vec2 radius;
	float height;
};

struct surface_point {
	zest_vec3 position;
	zest_vec3 normal;
	zest_vec2 uv;
	float direction;
	zest_vec3 direction_normal;
};

struct zest_widget_part {
	zest_bounding_box_t bb;
	zest_matrix4 transform_matrix;
	zest_uint group_id;
	float scale;
};

struct zest_widget {
	zest_vec3 position;
	zest_layer layer;
	zest_uint hovered_group_id;
	zest_widget_type type;
	zest_widget_part parts[6];
	zest_widget_part whole_widget;
};

struct particle {
	zest_vec3 position;
	zest_vec3 velocity;
	float speed;
	int id;
	float age;
};

struct particle2d {
	zest_vec3 position;
	zest_vec2 velocity;
	float age;
	float speed;
	int id;
};

typedef union {
	__m128 m;
	float a[4];
} tfxWideArray;

struct Quaternion {
	float w, x, y, z;

	Quaternion() : w(0.f), x(0.f), y(0.f), z(0.f) {}
	Quaternion(float w, float x, float y, float z) : w(w), x(x), y(y), z(z) {}

	// Normalize the quaternion
	Quaternion normalize() const {
		float len = sqrtf(w * w + x * x + y * y + z * z);
		return Quaternion(w / len, x / len, y / len, z / len);
	}

	float length() {
		float len = sqrtf(w * w + x * x + y * y + z * z);
		return len;
	}

	// Multiply quaternion by vector
	zest_vec3 rotate(const zest_vec3& v) const {
		// Convert zest_vec3 to Quaternion with w = 0
		Quaternion qv(0, v.x, v.y, v.z);

		// Calculate q * qv * q^-1
		Quaternion q_conjugate = Quaternion(w, -x, -y, -z);
		Quaternion result = (*this) * qv;
		result = result * q_conjugate;

		return {result.x, result.y, result.z};
	}

	// Quaternion multiplication
	Quaternion operator*(const Quaternion& q) const {
		return Quaternion(
			w * q.w - x * q.x - y * q.y - z * q.z,
			w * q.x + x * q.w + y * q.z - z * q.y,
			w * q.y - x * q.z + y * q.w + z * q.x,
			w * q.z + x * q.y - y * q.x + z * q.w
		);
	}

};

void TransformQuaternionVec3(const Quaternion* q, __m128* x, __m128* y, __m128* z) {
	__m128 qv_x = *x;
	__m128 qv_y = *y;
	__m128 qv_z = *z;
	__m128 qv_w = _mm_setzero_ps();

	__m128 q_x = _mm_set1_ps(q->x);
	__m128 q_y = _mm_set1_ps(q->y);
	__m128 q_z = _mm_set1_ps(q->z);
	__m128 q_w = _mm_set1_ps(q->w);

	__m128 two = _mm_set1_ps(2.0f);

	// Calculate t = 2 * cross(q.xyz, v)
	__m128 t_x = _mm_sub_ps(_mm_mul_ps(q_y, qv_z), _mm_mul_ps(q_z, qv_y));
	t_x = _mm_mul_ps(t_x, two);
	__m128 t_y = _mm_sub_ps(_mm_mul_ps(q_z, qv_x), _mm_mul_ps(q_x, qv_z));
	t_y = _mm_mul_ps(t_y, two);
	__m128 t_z = _mm_sub_ps(_mm_mul_ps(q_x, qv_y), _mm_mul_ps(q_y, qv_x));
	t_z = _mm_mul_ps(t_z, two);

	// Calculate v' = v + q.w * t + cross(q.xyz, t)
	__m128 qw_t_x = _mm_mul_ps(q_w, t_x);
	__m128 qw_t_y = _mm_mul_ps(q_w, t_y);
	__m128 qw_t_z = _mm_mul_ps(q_w, t_z);

	__m128 t_cross_x = _mm_sub_ps(_mm_mul_ps(q_y, t_z), _mm_mul_ps(q_z, t_y));
	__m128 t_cross_y = _mm_sub_ps(_mm_mul_ps(q_z, t_x), _mm_mul_ps(q_x, t_z));
	__m128 t_cross_z = _mm_sub_ps(_mm_mul_ps(q_x, t_y), _mm_mul_ps(q_y, t_x));

	*x = _mm_add_ps(_mm_add_ps(qv_x, qw_t_x), t_cross_x);
	*y = _mm_add_ps(_mm_add_ps(qv_y, qw_t_y), t_cross_y);
	*z = _mm_add_ps(_mm_add_ps(qv_z, qw_t_z), t_cross_z);
}

Quaternion ToQuaternion(float roll, float pitch, float yaw) // roll (x), pitch (y), yaw (z), angles are in radians
{
	// Abbreviations for the various angular functions

	float cr = cosf(roll * 0.5f);
	float sr = sinf(roll * 0.5f);
	float cp = cosf(pitch * 0.5f);
	float sp = sinf(pitch * 0.5f);
	float cy = cosf(yaw * 0.5f);
	float sy = sinf(yaw * 0.5f);

	Quaternion q;
	q.w = cr * cp * cy + sr * sp * sy;
	q.x = sr * cp * cy - cr * sp * sy;
	q.y = cr * sp * cy + sr * cp * sy;
	q.z = cr * cp * sy - sr * sp * cy;

	return q;
}

Quaternion FromDirection(const zest_vec3& dir) {
	zest_vec3 initial_dir = {0.0f, 0.0f, 1.0f};
	zest_vec3 normalized_dir = zest_NormalizeVec3(dir);

	zest_vec3 rotation_axis = zest_CrossProduct(initial_dir, normalized_dir);
	float dot_product = zest_DotProduct3(initial_dir, normalized_dir);
	float angle = acosf(dot_product);  // Angle between initial_dir and dir

	float half_angle = angle / 2.0f;
	float sin_half_angle = sinf(half_angle);

	return Quaternion(
		cosf(half_angle),
		rotation_axis.x * sin_half_angle,
		rotation_axis.y * sin_half_angle,
		rotation_axis.z * sin_half_angle
	).normalize();
}

float randomFloat(float min, float max) {
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_real_distribution<> dis(min, max);
	return (float)dis(gen);
}

zest_vec3 randomVectorInCone(const zest_vec3& coneDirection, float coneAngleDegrees) {
	// Convert cone angle to radians
	float coneAngle = zest_Radians(coneAngleDegrees);

	// Calculate the minimum z value for the cone
	float minZ = cos(coneAngle);

	// Randomly sample z in [minZ, 1]
	float z = randomFloat(minZ, 1.0f);

	// Randomly sample ϕ in [0, 2π)
	float phi = randomFloat(0.0f, 2.0f * ZEST_PI);

	// Calculate the corresponding x and y for the random point on the unit sphere
	float sqrtOneMinusZSquared = sqrt(1.0f - z * z);
	float x = sqrtOneMinusZSquared * cos(phi);
	float y = sqrtOneMinusZSquared * sin(phi);

	// Create the random vector in the cone's local space
	zest_vec3 randomVector = {x, y, z};

	// If the cone is centered around the north pole (0, 0, 1), return the vector as is
	if (coneDirection.x == 0 && coneDirection.y == 0) {
		return randomVector;
	}

	// Calculate the rotation axis (cross product of (0, 0, 1) and coneDirection)
	zest_vec3 northPole = { 0, 0, 1 };
	zest_vec3 rotationAxis = zest_CrossProduct(northPole, coneDirection);
	rotationAxis = zest_NormalizeVec3(rotationAxis);


	// Calculate the rotation angle (acos of dot product of (0, 0, 1) and coneDirection)
	float rotationAngle = acosf(zest_DotProduct3(northPole, zest_NormalizeVec3(coneDirection)));

	// Rotate the random vector to align with the cone direction
	// Use Rodrigues' rotation formula
	zest_vec3 xv = zest_ScaleVec3(randomVector, cos(rotationAngle));
	zest_vec3 yv = (zest_CrossProduct(rotationAxis, randomVector));
	yv = zest_ScaleVec3(yv, sin(rotationAngle));
	float zv_dot = zest_DotProduct3(rotationAxis, randomVector);
	zest_vec3 zv = zest_ScaleVec3(rotationAxis, zv_dot);
	zv = zest_ScaleVec3(zv, (1.f - cosf(rotationAngle)));
	zest_vec3 rotatedVector = zest_AddVec3(zest_AddVec3(xv, yv), zv);

	return rotatedVector;
}

zest_vec3 randomVectorInCone2(const zest_vec3& coneDirection, float coneAngleDegrees, zest_uint seed) {
	// Convert cone angle to radians
	float coneAngle = zest_Radians(coneAngleDegrees);

	// Calculate the minimum z value for the cone
	float minZ = cos(coneAngle);

	// Randomly sample z in [minZ, 1]
	float z = SeedGen(seed) / (float)UINT32_MAX;
	z = z - (minZ - 1.f) * z;

	// Randomly sample ϕ in [0, 2π)
	float phi = (SeedGen(seed + 1) / (float)UINT32_MAX) * (2.0f * ZEST_PI);

	// Calculate the corresponding x and y for the random point on the unit sphere
	float sqrtOneMinusZSquared = sqrt(1.0f - z * z);
	float x = sqrtOneMinusZSquared * cos(phi);
	float y = sqrtOneMinusZSquared * sin(phi);

	// Create the random vector in the cone's local space
	zest_vec3 randomVector = { x, y, z };

	// If the cone is centered around the north pole (0, 0, 1), return the vector as is
	if (coneDirection.x == 0 && coneDirection.y == 0) {
		return randomVector;
	}

	// Calculate the rotation axis (cross product of (0, 0, 1) and coneDirection)
	zest_vec3 northPole = { 0, 0, 1 };
	zest_vec3 rotationAxis = zest_CrossProduct(northPole, coneDirection);
	rotationAxis = zest_NormalizeVec3(rotationAxis);


	// Calculate the rotation angle (acos of dot product of (0, 0, 1) and coneDirection)
	float rotationAngle = acosf(zest_DotProduct3(northPole, zest_NormalizeVec3(coneDirection)));

	// Rotate the random vector to align with the cone direction
	// Use Rodrigues' rotation formula
	zest_vec3 xv = zest_ScaleVec3(randomVector, cos(rotationAngle));
	zest_vec3 yv = (zest_CrossProduct(rotationAxis, randomVector));
	yv = zest_ScaleVec3(yv, sin(rotationAngle));
	float zv_dot = zest_DotProduct3(rotationAxis, randomVector);
	zest_vec3 zv = zest_ScaleVec3(rotationAxis, zv_dot);
	zv = zest_ScaleVec3(zv, (1.f - cosf(rotationAngle)));
	zest_vec3 rotatedVector = zest_AddVec3(zest_AddVec3(xv, yv), zv);

	return rotatedVector;
}

struct ImGuiApp {
	zest_imgui_layer_info imgui_layer_info;
	zest_index imgui_draw_routine_index;
	zest_texture imgui_font_texture;
	zest_texture floor_texture;
	zest_texture sprite_texture;
	zest_image floor_image;
	zest_image sprite;
	zest_timer timer;
	zest_layer mesh_layer;
	zest_layer billboard_layer;
	zest_layer lines_2d;
	zest_layer line_layer;
	zest_pipeline mesh_pipeline;
	zest_pipeline line_pipeline;
	zest_pipeline line_2d_pipeline;
	zest_pipeline billboard_pipeline;

	zest_pipeline mesh_instance_pipeline;
	zest_layer move_widget_layer;
	zest_layer scale_widget_layer;
	zest_mesh_t mesh;
	zest_widget move_widget;
	zest_widget scale_widget;
	zest_widget_part *picked_widget_part = nullptr;
	zest_widget *picked_widget = nullptr;
	zest_vec3 first_intersection;
	zest_vec3 widget_dragged_amount;
	zest_vec3 clicked_widget_position;
	zest_uniform_buffer uniform_buffer_3d;
	zest_descriptor_set_layouts descriptor_layout;
	zest_descriptor_set_t descriptor_set;
	zest_vec3 plane_normals[6];
	zest_axis_flags current_axis;
	zest_vec3 angles;
	zest_vec3 velocity_normal;

	ValueNoise value_noise;
	float noise_frequency = 0.025f;
	float noise_influence = 0.05f;
	float speed = 0.f;
	float emission_direction = 0.f;
	float emission_range = 0.f;
	float time = 1000.f;
	zest_camera_t camera;
	ellipsoid ellipse;
	cylinder_t cylinder;
	surface_point points[1000];
	surface_point point;
	bool repoint = true;
	bool orthagonal = false;
	zest_vec3 cross_plane;
	zest_vec3 cube[8];

	particle particles[100];
	particle2d particles2d[100];

	float height_increment;
	float angle_increment;
	float radius;
	float spline_points;
};

void InitImGuiApp(ImGuiApp *app);

void DarkStyle2() {
	ImGui::GetStyle().FrameRounding = 4.0f;
	ImGui::GetStyle().GrabRounding = 4.0f;

	ImVec4* colors = ImGui::GetStyle().Colors;
	colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
	colors[ImGuiCol_TextDisabled] = ImVec4(1.00f, 1.00f, 1.00f, 0.26f);
	colors[ImGuiCol_WindowBg] = ImVec4(0.15f, 0.15f, 0.15f, 0.95f);
	colors[ImGuiCol_ChildBg] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
	colors[ImGuiCol_PopupBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
	colors[ImGuiCol_Border] = ImVec4(1.00f, 1.00f, 1.00f, 0.07f);
	colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.32f);
	colors[ImGuiCol_FrameBg] = ImVec4(0.11f, 0.11f, 0.11f, 1.00f);
	colors[ImGuiCol_FrameBgHovered] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
	colors[ImGuiCol_FrameBgActive] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
	colors[ImGuiCol_TitleBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
	colors[ImGuiCol_TitleBgActive] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
	colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
	colors[ImGuiCol_MenuBarBg] = ImVec4(0.15f, 0.15f, 0.15f, 0.95f);
	colors[ImGuiCol_ScrollbarBg] = ImVec4(0.02f, 0.02f, 0.02f, 0.34f);
	colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.86f, 0.31f, 0.02f, 1.00f);
	colors[ImGuiCol_CheckMark] = ImVec4(0.91f, 0.35f, 0.05f, 1.00f);
	colors[ImGuiCol_SliderGrab] = ImVec4(0.86f, 0.31f, 0.02f, 0.53f);
	colors[ImGuiCol_SliderGrabActive] = ImVec4(0.86f, 0.31f, 0.02f, 1.00f);
	colors[ImGuiCol_Button] = ImVec4(0.5f, 0.5f, 0.5f, .25f);
	colors[ImGuiCol_ButtonHovered] = ImVec4(0.86f, 0.31f, 0.02f, 0.53f);
	colors[ImGuiCol_ButtonActive] = ImVec4(0.86f, 0.31f, 0.02f, 1.00f);
	colors[ImGuiCol_Header] = ImVec4(0.00f, 0.00f, 0.00f, 0.36f);
	colors[ImGuiCol_HeaderHovered] = ImVec4(0.86f, 0.31f, 0.02f, 0.53f);
	colors[ImGuiCol_HeaderActive] = ImVec4(0.86f, 0.31f, 0.02f, 0.53f);
	colors[ImGuiCol_Separator] = ImVec4(0.00f, 0.00f, 0.00f, 0.22f);
	colors[ImGuiCol_SeparatorHovered] = ImVec4(0.86f, 0.31f, 0.02f, 0.53f);
	colors[ImGuiCol_SeparatorActive] = ImVec4(0.86f, 0.31f, 0.02f, 1.00f);
	colors[ImGuiCol_ResizeGrip] = ImVec4(0.86f, 0.31f, 0.02f, 0.53f);
	colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.86f, 0.31f, 0.02f, 0.53f);
	colors[ImGuiCol_ResizeGripActive] = ImVec4(0.86f, 0.31f, 0.02f, 1.00f);
	colors[ImGuiCol_Tab] = ImVec4(0.17f, 0.17f, 0.17f, 0.00f);
	colors[ImGuiCol_TabHovered] = ImVec4(0.86f, 0.31f, 0.02f, 0.80f);
	colors[ImGuiCol_TabSelected] = ImVec4(0.86f, 0.31f, 0.02f, 1.00f);
	colors[ImGuiCol_TabDimmed] = ImVec4(0.86f, 0.31f, 0.02f, 0.00f);
	colors[ImGuiCol_TabDimmedSelected] = ImVec4(0.86f, 0.31f, 0.02f, 0.68f);
	colors[ImGuiCol_DockingPreview] = ImVec4(0.86f, 0.31f, 0.02f, 1.00f);
	colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.00f);
	colors[ImGuiCol_PlotLines] = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
	colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
	colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
	colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
	colors[ImGuiCol_TableHeaderBg] = ImVec4(0.08f, 0.08f, 0.08f, 0.59f);
	colors[ImGuiCol_TableBorderStrong] = ImVec4(0.00f, 0.00f, 0.00f, 0.2f);
	colors[ImGuiCol_TableBorderLight] = ImVec4(0.00f, 0.00f, 0.00f, 0.1f);
	colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.00f, 0.00f, 0.00f, 0.11f);
	colors[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
	colors[ImGuiCol_DragDropTarget] = ImVec4(0.86f, 0.31f, 0.02f, 1.00f);
	colors[ImGuiCol_NavHighlight] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
	colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
	colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);

	ImGuiStyle &style = ImGui::GetStyle();
	style.WindowPadding.x = 10.f;
	style.WindowPadding.y = 10.f;
	style.FramePadding.x = 6.f;
	style.FramePadding.y = 4.f;
	style.ItemSpacing.x = 10.f;
	style.ItemSpacing.y = 6.f;
	style.ItemInnerSpacing.x = 5.f;
	style.ItemInnerSpacing.y = 5.f;
	style.WindowRounding = 0.f;
	style.FrameRounding = 4.f;
	style.ScrollbarRounding = 1.f;
	style.GrabRounding = 1.f;
	style.TabRounding = 4.f;
	style.IndentSpacing = 20.f;
	style.FrameBorderSize = 1.0f;

	style.ChildRounding = 0.f;
	style.PopupRounding = 0.f;
	style.CellPadding.x = 4.f;
	style.CellPadding.y = 2.f;
	style.TouchExtraPadding.x = 0.f;
	style.TouchExtraPadding.y = 0.f;
	style.ColumnsMinSpacing = 6.f;
	style.ScrollbarSize = 14.f;
	style.ScrollbarRounding = 1.f;
	style.GrabMinSize = 10.f;
	style.TabMinWidthForCloseButton = 0.f;
	style.DisplayWindowPadding.x = 19.f;
	style.DisplayWindowPadding.y = 19.f;
	style.DisplaySafeAreaPadding.x = 3.f;
	style.DisplaySafeAreaPadding.y = 3.f;
	style.MouseCursorScale = 1.f;
	style.WindowMinSize.x = 32.f;
	style.WindowMinSize.y = 32.f;
	style.ChildBorderSize = 1.f;

}
