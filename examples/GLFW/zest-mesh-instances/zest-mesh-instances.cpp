#include <zest.h>
#include "impl_glfw.h"
#include "impl_imgui_glfw.h"

struct MeshInstances {
	zest_layer mesh_layer;
};

void Init(MeshInstances* meshes) {

}

void Update(MeshInstances* meshes, float ellapsed) {

}

//Update callback called from Zest
void UpdateTfxExample(zest_microsecs ellapsed, void *data) {
	MeshInstances *meshes = static_cast<MeshInstances*>(data);
	Update(meshes, (float)ellapsed);
}

#if defined(_WIN32)
// Windows entry point
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow) {
//int main() {
	zest_vec3 v = zest_Vec3Set(1.f, 0.f, 0.f);
	zest_uint packed = zest_Pack8bitx3(&v);
	zest_create_info_t create_info = zest_CreateInfo();
	create_info.log_path = ".";
	ZEST__UNFLAG(create_info.flags, zest_init_flag_enable_vsync);
	zest_implglfw_SetCallbacks(&create_info);

	MeshInstances meshes;

	zest_Initialise(&create_info);
	zest_SetUserData(&meshes);
	zest_SetUserUpdateCallback(UpdateTfxExample);
	Init(&meshes);

	zest_Start();

	return 0;
}
#else
int main(void) {
	zest_vec3 v = zest_Vec3Set(1.f, 0.f, 0.f);
	zest_uint packed = zest_Pack8bitx3(&v);
	zest_create_info_t create_info = zest_CreateInfo();
	create_info.log_path = ".";
	ZEST__UNFLAG(create_info.flags, zest_init_flag_enable_vsync);
	zest_implglfw_SetCallbacks(&create_info);

	MeshInstances meshes;
	InitialiseTimelineFX(12);

	zest_Initialise(&create_info);
	zest_SetUserData(&meshes);
	zest_SetUserUpdateCallback(UpdateTfxExample);
	meshes.Init();

	zest_Start();

	return 0;
}
#endif
