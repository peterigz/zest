#include "zest-testing.h"
#include "imgui_internal.h"

//Update the uniform buffer used to transform vertices in the vertex buffer
void UpdateUniform3d(ImGuiApp* app) {
	zest_uniform_buffer_data_t* ubo_ptr = static_cast<zest_uniform_buffer_data_t*>(zest_GetUniformBufferData(ZestRenderer->standard_uniform_buffer));
	ubo_ptr->view = zest_LookAt(app->camera.position, zest_AddVec3(app->camera.position, app->camera.front), app->camera.up);
	ubo_ptr->proj = zest_Perspective(app->camera.fov, zest_ScreenWidthf() / zest_ScreenHeightf(), 0.001f, 10000.f);
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

zest_vec3 CylinderSurfaceNormal(float x, float z, float width, float depth) {
	// Calculate the gradient of the ellipse equation
	float dx = 2.f * x / (width * width);
	float dz = 2.f * z / (depth * depth);

	// Normalize the gradient vector to obtain the surface normal
	float length = sqrtf(dx * dx + dz * dz);
	zest_vec3 normal;
	normal.x = dx / length;
	normal.y = 0.f;
	normal.z = dz / length;

	return normal;
}

zest_vec3 CylinderSurfaceGradient(cylinder_t cylinder, float x, float z) {
	zest_vec3 gradient;

	zest_vec3 centerToPoint;
	centerToPoint.x = x;
	centerToPoint.y = 0.f; 
	centerToPoint.z = z;

	float magnitude_x = cylinder.radius.x / sqrtf(centerToPoint.x * centerToPoint.x + centerToPoint.z * centerToPoint.z);
	//float magnitude_y = cylinder.radius.y / sqrtf(centerToPoint.x * centerToPoint.x + centerToPoint.z * centerToPoint.z);

	// Calculate the gradient components
	gradient.x = magnitude_x * centerToPoint.x;
	gradient.z = magnitude_x * centerToPoint.z;
	gradient.y = 0.0;

	return gradient;
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

	zest_vec3 nearest_point;
	nearest_point.x = e.radius.x * x;
	nearest_point.y = e.radius.y * y;
	nearest_point.z = e.radius.z * z;

	return nearest_point;
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

zest_vec3 FindNearestPointOnPlane(zest_vec3 point, zest_vec3 plane_normal, float distance_from_origin) {
	zest_vec3 x; // Resultant closest point

	// Calculate v
	float sum_p = point.x * plane_normal.x + point.y * plane_normal.y + point.z * plane_normal.z;
	float sum_n = plane_normal.x * plane_normal.x + plane_normal.y * plane_normal.y + plane_normal.z * plane_normal.z;
	float v = (distance_from_origin - sum_p) / sum_n;

	// Calculate x
	x.x = point.x + v * plane_normal.x;
	x.y = point.y + v * plane_normal.y;
	x.z = point.z + v * plane_normal.z;

	return x;
}

zest_matrix4 RotationMatrix(zest_vec3 axis, float angle)
{
	float s = sin(angle);
	float c = cos(angle);
	float oc = 1.0 - c;
	return {	oc * axis.x * axis.x + c         , oc * axis.x * axis.y - axis.z * s, oc * axis.z * axis.x + axis.y * s, 0.f,
				oc * axis.x * axis.y + axis.z * s, oc * axis.y * axis.y + c         , oc * axis.y * axis.z - axis.x * s, 0.f,
				oc * axis.z * axis.x - axis.y * s, oc * axis.y * axis.z + axis.x * s, oc * axis.z * axis.z + c         , 0.f, 
				0.f                              , 0.f                              , 0.f                              , 0.f };
}

zest_vec3 CirclePointY(zest_vec2 center, float height, float angle) {
	float x = cosf(angle) * center.x + center.x;
	float z = -sinf(angle) * center.y + center.y;
	return { x, height, z };
}

zest_vec3 CirclePointX(zest_vec2 center, float height, float angle) {
	float x = cosf(angle) * center.x + center.x;
	float z = -sinf(angle) * center.y + center.y;
	return { x, height, z };
}

// Catmull-Rom spline interpolation function
zest_vec3 CatmullRomSpline(const zest_vec3& p0, const zest_vec3& p1, const zest_vec3& p2, const zest_vec3& p3, float t) {
	float t2 = t * t;
	float t3 = t2 * t;

	float b0 = 0.5f * (-t3 + 2.0f * t2 - t);
	float b1 = 0.5f * (3.0f * t3 - 5.0f * t2 + 2.0f);
	float b2 = 0.5f * (-3.0f * t3 + 4.0f * t2 + t);
	float b3 = 0.5f * (t3 - t2);

	float x = p0.x * b0 + p1.x * b1 + p2.x * b2 + p3.x * b3;
	float y = p0.y * b0 + p1.y * b1 + p2.y * b2 + p3.y * b3;

	return { x, y, 0.f };
}

zest_vec3 CatmullRomSpline3D(const zest_vec3& p0, const zest_vec3& p1, const zest_vec3& p2, const zest_vec3& p3, float t) {
	float t2 = t * t;
	float t3 = t2 * t;

	float b0 = 0.5f * (-t3 + 2.0f * t2 - t);
	float b1 = 0.5f * (3.0f * t3 - 5.0f * t2 + 2.0f);
	float b2 = 0.5f * (-3.0f * t3 + 4.0f * t2 + t);
	float b3 = 0.5f * (t3 - t2);

	float x = p0.x * b0 + p1.x * b1 + p2.x * b2 + p3.x * b3;
	float y = p0.y * b0 + p1.y * b1 + p2.y * b2 + p3.y * b3;
	float z = p0.z * b0 + p1.z * b1 + p2.z * b2 + p3.z * b3;

	return { x, y, z };
}

void CreatePathPoints(ImGuiApp *app) {
	float angle = 0.f;
	float height = 0.f;
	zest_vec3 point = CirclePointY({ app->radius, app->radius }, height, angle);
	for (int i = 0; i != 1000; ++i) {
		app->points[i].position = point;
		angle += app->angle_increment;
		height += app->height_increment;
		point = CirclePointY({ app->radius, app->radius }, height, angle);
	}
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
	zest_ProcessTextureImages(app->sprite_texture);
	app->mesh_pipeline = zest_Pipeline("pipeline_mesh");
	app->line_pipeline = zest_Pipeline("pipeline_line3d_instance");
	app->billboard_pipeline = zest_Pipeline("pipeline_billboard");
	app->mesh_instance_pipeline = zest_Pipeline("pipeline_mesh_instance");

	app->camera = zest_CreateCamera();
	zest_CameraPosition(&app->camera, { -10.f, 0.f, 0.f });
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
			app->mesh_instance_layer = zest_NewBuiltinLayerSetup("Instanced Mesh", zest_builtin_layer_mesh_instance);

			//Create a Dear ImGui layer
			zest_imgui_CreateLayer(&app->imgui_layer_info);
		}
		zest_FinishQueueSetup();
	}

	app->mesh = zest_CreateCylinderMesh(16, .25f, 5.f, zest_ColorSet(255, 0, 255, 255), 0);
	//app->mesh = zest_CreateCone(32, 1.f, 5.f);
	zest_mesh_t cone = zest_CreateCone(24, .5f, 2.f, zest_ColorSet(255, 100, 0, 255));
	zest_mesh_t sphere = zest_CreateSphere(16, 16, 1.f, zest_ColorSet(100, 255, 0, 255));
	zest_mesh_t cube = zest_CreateCube( 1.f, zest_ColorSet(255, 0, 0, 255));
	zest_mesh_t rounded = zest_CreateRoundedRectangle(4.f, 2.f, 0.25f, 8, 1, zest_ColorSet(255, 0, 0, 255));
	zest_PositionMesh(&cone, { 0.f, 2.5f, 0.f });
	zest_PositionMesh(&cube, { 0.f, 3.5f, 0.f });
	zest_AddMeshToMesh(&app->mesh, &cone);
	zest_AddMeshToMesh(&app->mesh, &sphere);
	zest_AddMeshToMesh(&app->mesh, &cube);
	zest_AddMeshToMesh(&app->mesh, &rounded);
	zest_AddMeshToLayer(app->mesh_instance_layer, &app->mesh);

	//Render specific - Set up the callback for updating the uniform buffers containing the model and view matrices
	UpdateUniform3d(app);

	//Set up a timer
	app->timer = zest_CreateTimer(60);
	/*
	app->ellipse = {};
	app->ellipse.radius.x = 5.f;
	app->ellipse.radius.y = 5.f;
	app->ellipse.radius.z = 10.f;
	for (int i = 0; i != 1000; ++i) {
		app->points[i] = {};
		app->points[i].uv.x = (float)i / (float)1000 * two_pi;
		app->points[i].uv.y = (float)rand() / (float)RAND_MAX;
		app->points[i].position = PointOnEllipse(app->points[i].uv.x, app->points[i].uv.y, app->ellipse.radius);
		app->points[i].normal = EllipseSurfaceNormal(app->points[i].position.x, app->points[i].position.y, app->points[i].position.z, app->ellipse.radius.x, app->ellipse.radius.y, app->ellipse.radius.z);
	}
	app->point = app->points[500];
	app->cross_plane = { 1.f, 0.f, 0.f };
	CreatePathPoints(app);
	app->height_increment = 0.1f;
	app->angle_increment = 0.1f;
	app->radius = 2.f;
	*/
	app->cylinder = {};
	app->cylinder.radius = { 5.f, 5.f };
	app->cylinder.height = 6.f;
	app->spline_points = 16.f;

	float grid_segment_size = two_pi / (app->spline_points - 2.f);
	float x = grid_segment_size;

	for (int i = 1; i != (int)app->spline_points - 1; ++i) {
		float th = x;
		app->points[i].position.x = cosf(th) * app->cylinder.radius.x;
		app->points[i].position.z = -sinf(th) * app->cylinder.radius.y;
		app->points[i].position.y = 0.f;
		x += grid_segment_size;
	}
	int i = (int)app->spline_points;
	app->points[0] = app->points[i - 2];
	app->points[i - 1] = app->points[1];
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
		/*
		ImGui::Text("u: %f, v: %f", app->point.uv.x, app->point.uv.y);
		ImGui::DragFloat3("Plane", &app->cross_plane.x, 0.01f, -1.f, 1.f);
		if (ImGui::IsItemEdited()) {
			app->cross_plane = zest_NormalizeVec3(app->cross_plane);
		}
		ImGui::DragFloat("direction", &app->point.direction, 0.01f);
		ImGui::DragFloat("Height Increment", &app->height_increment, 0.001f); if (ImGui::IsItemEdited) CreatePathPoints(app);
		ImGui::DragFloat("Angle Increment", &app->angle_increment, 0.001f); if (ImGui::IsItemEdited) CreatePathPoints(app);
		ImGui::DragFloat("Radius", &app->radius, 0.001f); if (ImGui::IsItemEdited) CreatePathPoints(app);
		*/

		zest_vec3 pole = {};
		zest_vec3 nearest_point = {};
		zest_vec3 perp = {};
		pole = { 0.f, 5.f, 0.f };
		nearest_point = FindNearestPointOnPlane(pole, app->point.normal, zest_LengthVec(app->point.position));
		perp = zest_SubVec3(nearest_point, app->point.position);
		perp = zest_NormalizeVec3(perp);
		zest_vec4 direction = { perp.x, perp.y, perp.z, 0.f };
		zest_matrix4 mat = RotationMatrix(app->point.normal, app->point.direction);
		zest_vec4 result = zest_MatrixTransformVector(&mat, direction);
		app->point.direction_normal = zest_NormalizeVec3({ result.x, result.y, result.z });
		//zest_imgui_DrawImage(app->test_image, 50.f, 50.f);
		ImGui::End();
		ImGui::Render();
		//Let the layer know that it needs to reupload the imgui mesh data to the GPU
		zest_SetLayerDirty(app->imgui_layer_info.mesh_layer);

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
		else {
			if (ImGui::IsKeyDown(ImGuiKey_W)) {
				app->point.position = zest_AddVec3(app->point.position, zest_ScaleVec3(&app->point.direction_normal, 0.1f));
				app->point.position = NearestPointOnEllipsoid(app->ellipse, app->point.position);
				app->point.normal = EllipseSurfaceNormal(app->point.position.x, app->point.position.y, app->point.position.z, app->ellipse.radius.x, app->ellipse.radius.y, app->ellipse.radius.z);
			} else if(ImGui::IsKeyDown(ImGuiKey_S)) {
				zest_vec3 perp = zest_CrossProduct(app->point.normal, app->cross_plane);
				app->point.position = zest_SubVec3(app->point.position, zest_ScaleVec3(&perp, 0.1f));
				app->point.position = NearestPointOnEllipsoid(app->ellipse, app->point.position);
				app->point.normal = EllipseSurfaceNormal(app->point.position.x, app->point.position.y, app->point.position.z, app->ellipse.radius.x, app->ellipse.radius.y, app->ellipse.radius.z);
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

	zest_SetMeshDrawing(app->mesh_layer, app->floor_texture, 0, app->mesh_pipeline);
	zest_DrawTexturedPlane(app->mesh_layer, app->floor_image, -500.f, -5.f, -500.f, 1000.f, 1000.f, 50.f, 50.f, 0.f, 0.f);

	zest_SetBillboardDrawing(app->billboard_layer, app->sprite_texture, 0, app->billboard_pipeline);

	zest_Set3DLineDrawing(app->line_layer, 0, app->line_pipeline);
	/*
	bool start_drawing = false;
	zest_vec3 p2;
	for (int i = 0; i != 97; ++i) {
		zest_vec3 point = app->points[i].position;
		zest_DrawBillboardSimple(app->billboard_layer, app->sprite, &point.x, 0.f, 0.2f, 0.2f);
		zest_vec3 start = app->points[i].position;
		zest_vec3 end = app->points[i + 1].position;
		//zest_Draw3DLine(app->line_layer, &start.x, &end.x, 3.f);
		float step = 0.1f; // Step size for interpolation
		for (float t = 0.0f; t <= 1.0f; t += step) {
			zest_vec3 p1 = CatmullRomSpline3D(app->points[i].position, app->points[i + 1].position, app->points[i + 2].position, app->points[i + 3].position, t);
			if (start_drawing == true) {
				zest_Draw3DLine(app->line_layer, &p1.x, &p2.x, 3.f);
			}
			start_drawing = true;
			p2 = p1;
		}
	}

	zest_SetLayerColor(app->line_layer, 255, 255, 255, 150);
	for (int i = 0; i != 1000; ++i) {
		zest_vec3 start = app->points[i].position;
		zest_vec3 end = zest_AddVec3(app->points[i].position, zest_ScaleVec3(&app->points[i].normal, .1f));
		zest_Draw3DLine(app->line_layer, &start.x, &end.x, 3.f);
	}
	zest_SetLayerColor(app->line_layer, 255, 50, 255, 255);
	zest_vec3 start = app->point.position;
	zest_vec3 end = zest_AddVec3(app->point.position, zest_ScaleVec3(&app->point.normal, 2.f));
	zest_Draw3DLine(app->line_layer, &start.x, &end.x, 5.f);
	zest_SetLayerColor(app->line_layer, 50, 200, 255, 255);
	zest_vec3 pole = { 0.f, 5.f, 0.f };
	zest_vec3 nearest_point = FindNearestPointOnPlane(pole, app->point.normal, zest_LengthVec(app->point.position));
	zest_vec3 perp = zest_SubVec3(nearest_point, app->point.position);
	start = app->point.position;
	end = zest_AddVec3(app->point.position, app->point.direction_normal);
	zest_Draw3DLine(app->line_layer, &start.x, &end.x, 5.f);
	//if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
	if (0) {
		app->point.uv.x = ZEST__CLAMP(two_pi * (zest_MouseXf() / zest_ScreenWidthf()), 0, two_pi);
		app->point.uv.y = ZEST__CLAMP(zest_MouseYf() / zest_ScreenHeight(), 0, 1.f);
		app->point.position = PointOnEllipse(app->point.uv.x, app->point.uv.y, app->ellipse.radius);
		app->point.normal = EllipseSurfaceNormal(app->point.position.x, app->point.position.y, app->point.position.z, app->ellipse.radius.x, app->ellipse.radius.y, app->ellipse.radius.z);
		app->repoint = true;
	}
	zest_vec3 nearest = NearestPointOnEllipsoid(app->ellipse, end);
	zest_SetLayerColor(app->line_layer, 255, 255, 100, 255);
	zest_Draw3DLine(app->line_layer, &end.x, &nearest.x, 5.f);
	start = { 0, 5.f, 0.f };
	zest_SetLayerColor(app->line_layer, 100, 200, 255, 255);
	zest_Draw3DLine(app->line_layer, &start.x, &app->point.position.x, 2.f);
	nearest_point = FindNearestPointOnPlane(start, app->point.normal, zest_LengthVec(app->point.position));
	zest_Draw3DLine(app->line_layer, &start.x, &nearest_point.x, 2.f);
	*/

	//Cylinder testing

	zest_SetLayerColor(app->line_layer, 255, 50, 255, 25);
	float grid_segment_size = two_pi / 100.f;
	zest_vec3 end;

	for (float x = 0; x != 100.f; ++x) {
		float th = x * grid_segment_size;
		float local_position_x = cosf(th) * app->cylinder.radius.x + app->cylinder.radius.x;
		float local_position_z = -sinf(th) * app->cylinder.radius.y + app->cylinder.radius.y;
		zest_vec3 start = { local_position_x, 0.f, local_position_z };
		end = start;
		end.y = app->cylinder.height;
		zest_Draw3DLine(app->line_layer, &start.x, &end.x, 2.f);
	}

	bool start_drawing = false;
	zest_vec3 p2;
	int c = (int)app->spline_points;
	int c_mod = c - 2;
	for (int i = 0; i != c - 1; ++i) {
		app->point.position = app->points[i].position;
		zest_vec3 tangent = {};
		tangent.x = app->point.position.z;
		tangent.z = -app->point.position.x;
		tangent = zest_NormalizeVec3(tangent);
		app->point.normal = CylinderSurfaceNormal(app->point.position.x - app->cylinder.radius.x, app->point.position.z - app->cylinder.radius.y, app->cylinder.radius.x, app->cylinder.radius.y);
		end = zest_AddVec3(app->point.position, tangent);
		zest_SetLayerColor(app->line_layer, 255, 255, 50, 255);
		zest_Draw3DLine(app->line_layer, &app->point.position.x, &end.x, 4.f);
		float step = 0.1f; // Step size for interpolation
		for (float t = 0.0f; t <= 1.0f; t += step) {
			zest_vec3 p1 = CatmullRomSpline3D(app->points[i % c_mod].position, app->points[(i + 1) % c_mod].position, app->points[(i + 2) % c_mod].position, app->points[(i + 3) % c_mod].position, t);
			if (start_drawing == true) {
				zest_Draw3DLine(app->line_layer, &p1.x, &p2.x, 3.f);
			}
			start_drawing = true;
			p2 = p1;
		}
	}

	zest_SetInstanceMeshDrawing(app->mesh_instance_layer, 0, app->mesh_instance_pipeline);
	zest_SetLayerColor(app->mesh_instance_layer, 255, 255, 255, 255);
	float scale = zest_MouseXf() / zest_ScreenWidthf();
	scale = 1.f;
	float rotation = zest_MouseYf() / zest_ScreenHeightf();
	zest_vec3 mesh_pos = { 0.f, 1.f, 0.f };
	zest_vec3 mesh_rot = { 0.f, 0.f, 0.f};
	zest_vec3 mesh_scale = { scale, scale, scale };
	zest_DrawInstancedMesh(app->mesh_instance_layer, &mesh_pos.x, &mesh_rot.x, &mesh_scale.x);

	//Load the imgui mesh data into the layer staging buffers. When the command queue is recorded, it will then upload that data to the GPU buffers for rendering
	zest_imgui_UpdateBuffers(app->imgui_layer_info.mesh_layer);
}

#if defined(_WIN32)
// Windows entry point
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow) {
//int main(void) {
	//Create new config struct for Zest
	zest_create_info_t create_info = zest_CreateInfo();
	ZEST__FLAG(create_info.flags, zest_init_flag_use_depth_buffer);
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
