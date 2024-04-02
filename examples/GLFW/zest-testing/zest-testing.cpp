#include "zest-testing.h"
#include "imgui_internal.h"

//Update the uniform buffer used to transform vertices in the vertex buffer
void UpdateUniform3d(ImGuiApp* app) {
	zest_uniform_buffer_data_t* ubo_ptr = static_cast<zest_uniform_buffer_data_t*>(zest_GetUniformBufferData(ZestRenderer->standard_uniform_buffer));
	ubo_ptr->view = zest_LookAt(app->camera.position, zest_AddVec3(app->camera.position, app->camera.front), app->camera.up);
	ubo_ptr->proj = zest_Perspective(app->camera.fov, zest_ScreenWidthf() / zest_ScreenHeightf(), 0.1f, 10000.f);
	ubo_ptr->proj.v[1].y *= -1.f;
	ubo_ptr->screen_size.x = zest_ScreenWidthf();
	ubo_ptr->screen_size.y = zest_ScreenHeightf();
	ubo_ptr->millisecs = 0;
}

//Function to project 2d screen coordinates into 3d space
zest_vec3 ScreenRay(float x, float y, float depth_offset, zest_vec3& camera_position, zest_descriptor_buffer buffer) {
	zest_uniform_buffer_data_t* buffer_3d = (zest_uniform_buffer_data_t*)zest_GetUniformBufferData(buffer);
	zest_vec3 camera_last_ray = zest_ScreenRay(x, y, zest_ScreenWidthf(), zest_ScreenHeightf(), &buffer_3d->proj, &buffer_3d->view);
	zest_vec3 pos = zest_AddVec3(zest_ScaleVec3(&camera_last_ray, depth_offset), camera_position);
	return { pos.x, pos.y, pos.z };
}

zest_vec3 EllipseSurfaceNormal(float x, float y, float z, float width, float height, float depth) {
	float dx = 2.f * x / (width * width);
	float dy = 2.f * y / (height * height);
	float dz = 2.f * z / (depth * depth);

	// Normalize the gradient vector to obtain the surface normal
	float length = sqrtf(dx * dx + dy * dy + dz * dz);
	zest_vec3 normal;
	normal.x = dx / length;
	normal.y = dy / length;
	normal.z = dz / length;

	return normal;
}

zest_vec3 PointOnEllipse(float theta, float v, zest_vec3 radius) {
	float phi = acosf(2.f * v - 1.f);
	float sin_theta = sinf(theta);
	float cos_theta = cosf(theta);
	float sin_phi = sinf(phi);
	float cos_phi = cosf(phi);

	zest_vec3 position;
	position.x = radius.x * sin_phi * cos_theta;
	position.y = radius.y * sin_phi * sin_theta;
	position.z = radius.z * cos_phi;
	return position;
}

zest_vec3 NearestPointOnEllipsoid(ellipsoid e, zest_vec3 point) {
	float x = point.x / e.radius.x;
	float y = point.y / e.radius.y;
	float z = point.z / e.radius.z;

	float length = sqrtf(x * x + y * y + z * z);
	x /= length;
	y /= length;
	z /= length;

	zest_vec3 nearestPoint;
	nearestPoint.x = e.radius.x * x;
	nearestPoint.y = e.radius.y * y;
	nearestPoint.z = e.radius.z * z;

	return nearestPoint;
}

zest_vec3 RotateVectorOnPlane(const zest_vec3& planeNormal, const zest_vec3& vectorOnPlane) {
	// Compute rotation angle
	float dotProduct = planeNormal.y; // Assuming planeNormal is already normalized
	float rotationAngle = acosf(dotProduct);

	// Compute rotation axis
	zest_vec3 rotationAxis = { planeNormal.z, 0.0f, -planeNormal.x }; // Cross product with {0, 1, 0}

	// Rotate the vector
	float sinAngle = sinf(rotationAngle);
	float cosAngle = cosf(rotationAngle);
	zest_vec3 rotated_vector;
	rotated_vector.x = vectorOnPlane.x * cosAngle - vectorOnPlane.z * sinAngle;
	rotated_vector.y = vectorOnPlane.y;
	rotated_vector.z = vectorOnPlane.x * sinAngle + vectorOnPlane.z * cosAngle;

	return rotated_vector;
}

void InitImGuiApp(ImGuiApp* app) {
	//Initialise Dear ImGui
	zest_imgui_Initialise(&app->imgui_layer_info);
	//Implement a dark style
	DarkStyle2();

	//This is an exmaple of how to change the font that ImGui uses
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.Fonts->Clear();
	float font_size = 15.f;
	unsigned char* font_data;
	int tex_width, tex_height;
	ImFontConfig config;
	config.PixelSnapH = true;
	io.Fonts->AddFontFromFileTTF("examples/assets/Lato-Regular.ttf", font_size);
	io.Fonts->GetTexDataAsRGBA32(&font_data, &tex_width, &tex_height);

	//Rebuild the Zest font texture
	zest_imgui_RebuildFontTexture(&app->imgui_layer_info, tex_width, tex_height, font_data);

	app->floor_texture = zest_CreateTexture("Floor texture", zest_texture_storage_type_bank, zest_texture_flag_use_filtering, zest_texture_format_rgba, 10);
	app->floor_image = zest_AddTextureImageFile(app->floor_texture, "examples/assets/checker.png");
	zest_ProcessTextureImages(app->floor_texture);
	app->sprite_texture = zest_CreateTexture("Sprite texture", zest_texture_storage_type_bank, zest_texture_flag_use_filtering, zest_texture_format_rgba, 10);
	app->sprite = zest_AddTextureImageFile(app->sprite_texture, "examples/assets/wabbit_alpha.png");
	zest_ProcessTextureImages(app->floor_texture);
	app->mesh_pipeline = zest_Pipeline("pipeline_mesh");
	app->line_pipeline = zest_Pipeline("pipeline_line3d_instance");

	app->camera = zest_CreateCamera();
	zest_CameraPosition(&app->camera, { 0.f, 0.f, 0.f });
	zest_CameraSetFoV(&app->camera, 60.f);
	zest_CameraSetYaw(&app->camera, zest_Radians(-90.f));
	zest_CameraSetPitch(&app->camera, zest_Radians(0.f));
	zest_CameraUpdateFront(&app->camera);

	zest_ModifyCommandQueue(ZestApp->default_command_queue);
	{
		zest_ModifyDrawCommands(ZestApp->default_draw_commands);
		{
			//Create a new mesh layer in the command queue to draw the floor plane
			app->mesh_layer = zest_NewBuiltinLayerSetup("Meshes", zest_builtin_layer_mesh);
			app->billboard_layer = zest_NewBuiltinLayerSetup("Billboards", zest_builtin_layer_billboards);
			app->line_layer = zest_NewBuiltinLayerSetup("Lines", zest_builtin_layer_3dlines);

			//Create a Dear ImGui layer
			zest_imgui_CreateLayer(&app->imgui_layer_info);
		}
		zest_FinishQueueSetup();
	}

	//Render specific - Set up the callback for updating the uniform buffers containing the model and view matrices
	UpdateUniform3d(app);

	//Set up a timer
	app->timer = zest_CreateTimer(60);
	app->ellipse = {};
	app->ellipse.radius.x = 5.f;
	app->ellipse.radius.y = 5.f;
	app->ellipse.radius.z = 5.f;
	for (int i = 0; i != 1000; ++i) {
		app->points[i].uv.x = (float)i / (float)1000 * two_pi;
		app->points[i].uv.y = (float)rand() / (float)RAND_MAX;
		app->points[i].position = PointOnEllipse(app->points[i].uv.x, app->points[i].uv.y, app->ellipse.radius);
		app->points[i].normal = EllipseSurfaceNormal(app->points[i].position.x, app->points[i].position.y, app->points[i].position.z, app->ellipse.radius.x, app->ellipse.radius.y, app->ellipse.radius.z);
	}
	app->point = app->points[500];
}

void UpdateCallback(zest_microsecs elapsed, void* user_data) {
	//Don't forget to update the uniform buffer!
	//Set the active command queue to the default one that was created when Zest was initialised
	zest_SetActiveCommandQueue(ZestApp->default_command_queue);
	ImGuiApp* app = (ImGuiApp*)user_data;
	UpdateUniform3d(app);

	//First control the camera with the mosue if the right mouse is clicked
	bool camera_free_look = false;
	if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
		camera_free_look = true;
		if (glfwRawMouseMotionSupported()) {
			glfwSetInputMode((GLFWwindow*)zest_Window(), GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
		}
		ZEST__FLAG(ImGui::GetIO().ConfigFlags, ImGuiConfigFlags_NoMouse);
		double x_mouse_speed;
		double y_mouse_speed;
		zest_GetMouseSpeed(&x_mouse_speed, &y_mouse_speed);
		zest_TurnCamera(&app->camera, (float)ZestApp->mouse_delta_x, (float)ZestApp->mouse_delta_y, .05f);
	}
	else if (glfwRawMouseMotionSupported()) {
		camera_free_look = false;
		ZEST__UNFLAG(ImGui::GetIO().ConfigFlags, ImGuiConfigFlags_NoMouse);
		glfwSetInputMode((GLFWwindow*)zest_Window(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	}
	else {
		camera_free_look = false;
		ZEST__UNFLAG(ImGui::GetIO().ConfigFlags, ImGuiConfigFlags_NoMouse);
	}

	zest_TimerAccumulate(app->timer);
	while (zest_TimerDoUpdate(app->timer)) {
		//Must call the imgui GLFW implementation function
		ImGui_ImplGlfw_NewFrame();
		//Draw our imgui stuff
		ImGui::NewFrame();
		ImGui::Begin("Test Window");
		ImGui::Text("FPS %i", ZestApp->last_fps);
		ImGui::Text("u: %f, v: %f", app->point.uv.x, app->point.uv.y);
		//zest_imgui_DrawImage(app->test_image, 50.f, 50.f);
		ImGui::End();
		ImGui::Render();
		//Let the layer know that it needs to reupload the imgui mesh data to the GPU
		zest_SetLayerDirty(app->imgui_layer_info.mesh_layer);
		//Load the imgui mesh data into the layer staging buffers. When the command queue is recorded, it will then upload that data to the GPU buffers for rendering
		zest_imgui_UpdateBuffers(app->imgui_layer_info.mesh_layer);

		float speed = 5.f * (float)app->timer->update_time;
		if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
			ImGui::SetWindowFocus(nullptr);

			if (ImGui::IsKeyDown(ImGuiKey_W)) {
				zest_CameraMoveForward(&app->camera, speed);
			}
			if (ImGui::IsKeyDown(ImGuiKey_S)) {
				zest_CameraMoveBackward(&app->camera, speed);
			}
			if (ImGui::IsKeyDown(ImGuiKey_UpArrow)) {
				zest_CameraMoveUp(&app->camera, speed);
			}
			if (ImGui::IsKeyDown(ImGuiKey_DownArrow)) {
				zest_CameraMoveDown(&app->camera, speed);
			}
			if (ImGui::IsKeyDown(ImGuiKey_A)) {
				zest_CameraStrafLeft(&app->camera, speed);
			}
			if (ImGui::IsKeyDown(ImGuiKey_D)) {
				zest_CameraStrafRight(&app->camera, speed);
			}
		}

		//Restore the mouse when right mouse isn't held down
		if (camera_free_look) {
			glfwSetInputMode((GLFWwindow*)zest_Window(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		}
		else {
			glfwSetInputMode((GLFWwindow*)zest_Window(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		}

		zest_TimerUnAccumulate(app->timer);
	}
	zest_TimerSet(app->timer);

	//zest_SetMeshDrawing(app->mesh_layer, app->floor_texture, 0, app->mesh_pipeline);
	//zest_DrawTexturedPlane(app->mesh_layer, app->floor_image, -500.f, -5.f, -500.f, 1000.f, 1000.f, 50.f, 50.f, 0.f, 0.f);

	zest_Set3DLineDrawing(app->line_layer, 0, app->line_pipeline);
	zest_SetLayerColor(app->line_layer, 255, 255, 255, 50);
	for (int i = 0; i != 1000; ++i) {
		zest_vec3 start = app->points[i].position;
		zest_vec3 end = zest_AddVec3(app->points[i].position, zest_ScaleVec3(&app->points[i].normal, .1f));
		zest_Draw3DLine(app->line_layer, &start.x, &end.x, 3.f);
	}
	zest_SetLayerColor(app->line_layer, 255, 50, 255, 255);
	if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
		int i = ZEST__CLAMP(int(1000.f * (zest_MouseXf() / zest_ScreenWidth())), 0, 999);
		app->point = app->points[i];
	}
	zest_vec3 start = app->point.position;
	zest_vec3 end = zest_AddVec3(app->point.position, zest_ScaleVec3(&app->point.normal, 2.f));
	zest_Draw3DLine(app->line_layer, &start.x, &end.x, 5.f);
	zest_SetLayerColor(app->line_layer, 50, 200, 255, 255);
	zest_vec3 np = { 0.f, 1.f, 0.f };
	if (!ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
	}
	zest_vec3 perp = zest_CrossProduct(app->point.normal, np);
	perp = zest_NormalizeVec3(perp);
	start = app->point.position;
	end = zest_AddVec3(app->point.position, zest_ScaleVec3(&perp, 2.f));
	zest_Draw3DLine(app->line_layer, &start.x, &end.x, 5.f);
	if (!ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
		app->point.uv.x = ZEST__CLAMP(two_pi * (zest_MouseXf() / zest_ScreenWidthf()), 0, two_pi);
		app->point.uv.y = ZEST__CLAMP(zest_MouseYf() / zest_ScreenHeight(), 0, 1.f);
		app->point.position = PointOnEllipse(app->point.uv.x, app->point.uv.y, app->ellipse.radius);
		app->point.normal = EllipseSurfaceNormal(app->point.position.x, app->point.position.y, app->point.position.z, app->ellipse.radius.x, app->ellipse.radius.y, app->ellipse.radius.z);
	}
	else {
		perp = RotateVectorOnPlane(app->point.normal, perp);
		app->point.position = zest_AddVec3(app->point.position, zest_ScaleVec3(&perp, 0.1f));
		app->point.position = NearestPointOnEllipsoid(app->ellipse, app->point.position);
		app->point.normal = EllipseSurfaceNormal(app->point.position.x, app->point.position.y, app->point.position.z, app->ellipse.radius.x, app->ellipse.radius.y, app->ellipse.radius.z);
	}
	zest_vec3 nearest = NearestPointOnEllipsoid(app->ellipse, end);
	zest_SetLayerColor(app->line_layer, 255, 255, 100, 255);
	zest_Draw3DLine(app->line_layer, &end.x, &nearest.x, 5.f);
}

#if defined(_WIN32)
// Windows entry point
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow) {
//int main(void) {
	//Create new config struct for Zest
	zest_create_info_t create_info = zest_CreateInfo();
	//Don't enable vsync so we can see the FPS go higher then the refresh rate
	//ZEST__UNFLAG(create_info.flags, zest_init_flag_enable_vsync);
	//Implement GLFW for window creation
	zest_implglfw_SetCallbacks(&create_info);

	ImGuiApp imgui_app;

	//Initialise Zest
	zest_Initialise(&create_info);
	//Set the Zest use data
	zest_SetUserData(&imgui_app);
	//Set the udpate callback to be called every frame
	zest_SetUserUpdateCallback(UpdateCallback);
	//Initialise our example
	InitImGuiApp(&imgui_app);

	//Start the main loop
	zest_Start();

	return 0;
}
#else
int main(void) {
	zest_create_info_t create_info = zest_CreateInfo();
	zest_implglfw_SetCallbacks(&create_info);
    ZEST__FLAG(create_info.flags, zest_init_flag_maximised);

	ImGuiApp imgui_app;

    create_info.log_path = ".";
	zest_Initialise(&create_info);
	zest_SetUserData(&imgui_app);
	zest_SetUserUpdateCallback(UpdateCallback);
	InitImGuiApp(&imgui_app);

	zest_Start();

	return 0;
}
#endif
