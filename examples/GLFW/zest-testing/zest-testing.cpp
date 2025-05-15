#include "zest-testing.h"
#include "imgui_internal.h"

//Update the uniform buffer used to transform vertices in the vertex buffer
void UpdateUniform3d(ImGuiApp* app) {
	zest_uniform_buffer_data_t* ubo_ptr = static_cast<zest_uniform_buffer_data_t*>(zest_GetUniformBufferData(ZestRenderer->standard_uniform_buffer));

	if (app->orthagonal == true) {
		zest_vec3 front = zest_NormalizeVec3(app->camera.position);
		front = zest_FlipVec3(front);
		ubo_ptr->view = zest_LookAt(app->camera.position, zest_AddVec3(app->camera.position, front), app->camera.up);
		float ratio = zest_ScreenWidthf() / zest_ScreenHeightf();
		ubo_ptr->proj = zest_Ortho(-(app->camera.ortho_scale * ratio), app->camera.ortho_scale * ratio, -app->camera.ortho_scale, app->camera.ortho_scale, -1000.f, 10000.f);
	}
	else {
		ubo_ptr->view = zest_LookAt(app->camera.position, zest_AddVec3(app->camera.position, app->camera.front), app->camera.up);
		ubo_ptr->proj = zest_Perspective(app->camera.fov, zest_ScreenWidthf() / zest_ScreenHeightf(), 0.001f, 10000.f);
	}
	ubo_ptr->proj.v[1].y *= -1.f;
	ubo_ptr->screen_size.x = zest_ScreenWidthf();
	ubo_ptr->screen_size.y = zest_ScreenHeightf();
	ubo_ptr->millisecs = zest_Millisecs() % 300;
}

//Function to project 2d screen coordinates into 3d space
zest_vec3 ScreenRay(float x, float y, float depth_offset, zest_vec3& camera_position, zest_descriptor_buffer buffer) {
	zest_uniform_buffer_data_t* buffer_3d = (zest_uniform_buffer_data_t*)zest_GetUniformBufferData(buffer);
	zest_vec3 camera_last_ray = zest_ScreenRay(x, y, zest_ScreenWidthf(), zest_ScreenHeightf(), &buffer_3d->proj, &buffer_3d->view);
	zest_vec3 pos = zest_AddVec3(zest_ScaleVec3(camera_last_ray, depth_offset), camera_position);
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
	float s = sinf(angle);
	float c = cosf(angle);
	float oc = 1.f - c;
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

bool PickWidget(zest_widget_part* widget, zest_vec3 ray_origin, zest_vec3 ray_direction) {
	float tMin = 0.0f;
	float tMax = 100000.0f;

	zest_vec3 pos = zest_Vec3Set(widget->transform_matrix.v[0].w, widget->transform_matrix.v[1].w, widget->transform_matrix.v[2].w);
	zest_vec3 scale = zest_Vec3Set(widget->transform_matrix.v[3].x, widget->transform_matrix.v[3].y, widget->transform_matrix.v[3].z);
	//pos = zest_MulVec3(&pos, &scale);

	zest_vec3 delta = zest_SubVec3(pos, ray_origin);

	zest_vec3 axis = zest_Vec3Set(widget->transform_matrix.v[0].x, widget->transform_matrix.v[1].x, widget->transform_matrix.v[2].x);
	float e = zest_DotProduct3(axis, delta);
	float f = zest_DotProduct3(ray_direction, axis);

	// Beware, don't do the division if f is near 0 ! See full source code for details.
	float t1 = (e + widget->bb.min_bounds.x * scale.x) / f; // Intersection with the "left" plane
	float t2 = (e + widget->bb.max_bounds.x * scale.x) / f; // Intersection with the "right" plane

	if (t1 > t2) { // if wrong order
		float w = t1; t1 = t2; t2 = w; // swap t1 and t2
	}

	// tMax is the nearest "far" intersection (amongst the X,Y and Z planes pairs)
	if (t2 < tMax) tMax = t2;
	// tMin is the farthest "near" intersection (amongst the X,Y and Z planes pairs)
	if (t1 > tMin) tMin = t1;

	if (tMax < tMin) {
		return false;
	}

	axis = zest_Vec3Set(widget->transform_matrix.v[0].y, widget->transform_matrix.v[1].y, widget->transform_matrix.v[2].y);
	e = zest_DotProduct3(axis, delta);
	f = zest_DotProduct3(ray_direction, axis);

	// Beware, don't do the division if f is near 0 ! See full source code for details.
	t1 = (e + widget->bb.min_bounds.y * scale.y) / f; // Intersection with the "left" plane
	t2 = (e + widget->bb.max_bounds.y * scale.y) / f; // Intersection with the "right" plane

	if (t1 > t2) { // if wrong order
		float w = t1; t1 = t2; t2 = w; // swap t1 and t2
	}

	// tMax is the nearest "far" intersection (amongst the X,Y and Z planes pairs)
	if (t2 < tMax) tMax = t2;
	// tMin is the farthest "near" intersection (amongst the X,Y and Z planes pairs)
	if (t1 > tMin) tMin = t1;

	if (tMax < tMin) {
		return false;
	}

	axis = zest_Vec3Set(widget->transform_matrix.v[0].z, widget->transform_matrix.v[1].z, widget->transform_matrix.v[2].z);
	e = zest_DotProduct3(axis, delta);
	f = zest_DotProduct3(ray_direction, axis);

	// Beware, don't do the division if f is near 0 ! See full source code for details.
	t1 = (e + widget->bb.min_bounds.z * scale.z) / f; // Intersection with the "left" plane
	t2 = (e + widget->bb.max_bounds.z * scale.z) / f; // Intersection with the "right" plane

	if (t1 > t2) { // if wrong order
		float w = t1; t1 = t2; t2 = w; // swap t1 and t2
	}

	// tMax is the nearest "far" intersection (amongst the X,Y and Z planes pairs)
	if (t2 < tMax) tMax = t2;
	// tMin is the farthest "near" intersection (amongst the X,Y and Z planes pairs)
	if (t1 > tMin) tMin = t1;

	if (tMax < tMin) {
		return false;
	}

	return true;
}

void SetWidgetPartScale(zest_widget *widget, float scale) {
	for (int i = 0; i != zest_part_none; ++i) {
		widget->parts[i].transform_matrix.v[3].x = scale;
		widget->parts[i].transform_matrix.v[3].y = scale;
		widget->parts[i].transform_matrix.v[3].z = scale;
	}
	widget->whole_widget.transform_matrix.v[3].x = scale;
	widget->whole_widget.transform_matrix.v[3].y = scale;
	widget->whole_widget.transform_matrix.v[3].z = scale;
}

void SetWidgetPosition(zest_widget *widget, zest_vec3 position) {
	for (int i = 0; i != zest_part_none; ++i) {
		widget->parts[i].transform_matrix.v[0].w = position.x;
		widget->parts[i].transform_matrix.v[1].w = position.y;
		widget->parts[i].transform_matrix.v[2].w = position.z;
	}
	widget->whole_widget.transform_matrix.v[0].w = position.x;
	widget->whole_widget.transform_matrix.v[1].w = position.y;
	widget->whole_widget.transform_matrix.v[2].w = position.z;
	widget->position.x = position.x;
	widget->position.y = position.y;
	widget->position.z = position.z;
}

zest_vec3 GetWidgetPosition(zest_widget_part* widget) {
	zest_vec3 pos = zest_Vec3Set(widget->transform_matrix.v[0].w, widget->transform_matrix.v[1].w, widget->transform_matrix.v[2].w);
	return pos;
}

zest_vec3 GetWidgetAxisX(zest_widget_part* widget) {
	zest_vec3 rotation = zest_Vec3Set(widget->transform_matrix.v[0].x, widget->transform_matrix.v[1].x, widget->transform_matrix.v[2].x);
	return rotation;
}

zest_vec3 GetWidgetAxisY(zest_widget_part* widget) {
	zest_vec3 rotation = zest_Vec3Set(widget->transform_matrix.v[0].y, widget->transform_matrix.v[1].y, widget->transform_matrix.v[2].y);
	return rotation;
}

zest_vec3 GetWidgetAxisZ(zest_widget_part* widget) {
	zest_vec3 rotation = zest_Vec3Set(widget->transform_matrix.v[0].z, widget->transform_matrix.v[1].z, widget->transform_matrix.v[2].z);
	return rotation;
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

	app->floor_texture = zest_CreateTexture("Floor texture", zest_texture_storage_type_bank, zest_texture_flag_use_filtering, zest_texture_format_rgba_unorm, 10);
	app->floor_image = zest_AddTextureImageFile(app->floor_texture, "examples/assets/checker.png");
	zest_ProcessTextureImages(app->floor_texture);
	app->sprite_texture = zest_CreateTexture("Sprite texture", zest_texture_storage_type_bank, zest_texture_flag_use_filtering, zest_texture_format_rgba_unorm, 10);
	app->sprite = zest_AddTextureImageFile(app->sprite_texture, "examples/assets/wabbit_alpha.png");
	zest_ProcessTextureImages(app->sprite_texture);
	app->mesh_pipeline = zest_Pipeline("pipeline_mesh");
	app->line_pipeline = zest_Pipeline("pipeline_line3d_instance");
	app->line_2d_pipeline = zest_Pipeline("pipeline_line_instance");
	app->billboard_pipeline = zest_Pipeline("pipeline_billboard");

	app->billboard_shader_resources = zest_CombineUniformAndTextureSampler(ZestRenderer->uniform_descriptor_set, app->sprite_texture);
	app->floor_shader_resources = zest_CombineUniformAndTextureSampler(ZestRenderer->uniform_descriptor_set, app->floor_texture);
	app->mesh_shader_resources = zest_CreateShaderResources();
	zest_AddDescriptorSetToResources(app->mesh_shader_resources, ZestRenderer->uniform_descriptor_set);

	zest_pipeline_template_create_info_t custom_mesh_pipeline = zest_CopyTemplateFromPipeline("pipeline_mesh_instance");
	app->mesh_instance_pipeline = zest_CreatePipelineTemplate("pipeline_mesh_instance_custom");
	zest_SetPipelineTemplateShader(&custom_mesh_pipeline, "mesh_instance_custom.spv", "examples/assets/spv/");
	zest_FinalisePipelineTemplate(app->mesh_instance_pipeline, zest_GetStandardRenderPass(), &custom_mesh_pipeline);
	zest_BuildPipeline(app->mesh_instance_pipeline);
    zest_MakePipelineDescriptorWrites(app->mesh_instance_pipeline);

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
			app->move_widget_layer = zest_NewBuiltinLayerSetup("Move Widget", zest_builtin_layer_mesh_instance);
			app->scale_widget_layer = zest_NewBuiltinLayerSetup("Scale Widget", zest_builtin_layer_mesh_instance);
			app->lines_2d = zest_NewBuiltinLayerSetup("Lines 2d", zest_builtin_layer_lines);

			//Create a Dear ImGui layer
			zest_imgui_CreateLayer(&app->imgui_layer_info);
		}
		zest_FinishQueueSetup();
	}

	float arrow_radius = 0.01f;
	float point_radius = 0.05f;
	float arrow_length = .5f;
	float point_length = .1f;
	float point_offset = .35f;
	float arrow_offset = arrow_length * .5f;
	float plane_size = 0.25f;
	float plane_radius = 0.025f;

	//Move Widget Setup
	zest_mesh_t x_arrow = zest_CreateCylinderMesh(8, arrow_radius, arrow_length, zest_ColorSet(255, 0, 0, 255), 0);
	zest_mesh_t x_arrow_pointer = zest_CreateCone(8, point_radius, point_length, zest_ColorSet(255, 0, 0, 255));
	zest_mesh_t x_arrow_block = zest_CreateCube(point_radius * 2.f, zest_ColorSet(255, 0, 0, 255));
	zest_mesh_t y_arrow = zest_CreateCylinderMesh(8, arrow_radius, arrow_length, zest_ColorSet(0, 255, 0, 255), 0);
	zest_mesh_t y_arrow_pointer = zest_CreateCone(8, point_radius, point_length, zest_ColorSet(0, 255, 0, 255));
	zest_mesh_t y_arrow_block = zest_CreateCube(point_radius * 2.f, zest_ColorSet(0, 255, 0, 255));
	zest_mesh_t z_arrow = zest_CreateCylinderMesh(8, arrow_radius, arrow_length, zest_ColorSet(0, 0, 255, 255), 0);
	zest_mesh_t z_arrow_pointer = zest_CreateCone(8, point_radius, point_length, zest_ColorSet(0, 0, 255, 255));
	zest_mesh_t z_arrow_block = zest_CreateCube(point_radius * 2.f, zest_ColorSet(0, 0, 255, 255));
	zest_mesh_t x_plane_mesh = zest_CreateRoundedRectangle(plane_size, plane_size, plane_radius, 8, 1, zest_ColorSet(255, 0, 0, 255));
	zest_mesh_t y_plane_mesh = zest_CreateRoundedRectangle(plane_size, plane_size, plane_radius, 8, 1, zest_ColorSet(0, 0, 255, 255));
	zest_mesh_t z_plane_mesh = zest_CreateRoundedRectangle(plane_size, plane_size, plane_radius, 8, 1, zest_ColorSet(0, 255, 0, 255));
	app->move_widget.position = { 0 };
	zest_TransformMesh(&x_arrow, zest_Radians(90.f), 0.f, 0.f, 0.f, 0.f, point_offset, 1.f, 1.f, 1.f);
	zest_TransformMesh(&x_arrow_pointer, zest_Radians(90.f), 0.f, 0.f, 0.f, 0.f, point_offset + arrow_offset, 1.f, 1.f, 1.f);
	zest_TransformMesh(&x_arrow_block, 0.f, 0.f, 0.f, 0.f, 0.f, point_offset + arrow_offset, 1.f, 1.f, 1.f);
	zest_TransformMesh(&y_arrow, 0.f, 0.f, 0.f, 0.f, point_offset, 0.f, 1.f, 1.f, 1.f);
	zest_TransformMesh(&y_arrow_pointer, 0.f, 0.f, 0.f, 0.f, arrow_offset + point_offset, 0.f, 1.f, 1.f, 1.f);
	zest_TransformMesh(&y_arrow_block, 0.f, 0.f, 0.f, 0.f, arrow_offset + point_offset, 0.f, 1.f, 1.f, 1.f);
	zest_TransformMesh(&z_arrow, 0.f, 0.f, zest_Radians(90.f), -point_offset, 0.f, 0.f, 1.f, 1.f, 1.f);
	zest_TransformMesh(&z_arrow_pointer, 0.f, 0.f, zest_Radians(90.f), -(arrow_offset + point_offset), 0.f, 0.f, 1.f, 1.f, 1.f);
	zest_TransformMesh(&z_arrow_block, 0.f, 0.f, 0.f, -(arrow_offset + point_offset), 0.f, 0.f, 1.f, 1.f, 1.f);
	zest_TransformMesh(&x_plane_mesh, 0.f, zest_Radians(90.f), 0.f, 0.f, point_offset, point_offset, 1.f, 1.f, 1.f);
	zest_TransformMesh(&z_plane_mesh, zest_Radians(90.f), 0.f, 0.f, -point_offset, 0.f, point_offset, 1.f, 1.f, 1.f);
	zest_TransformMesh(&y_plane_mesh, 0.f, 0.f, 0.f, -point_offset, point_offset, 0.f, 1.f, 1.f, 1.f);
	zest_mesh_t x_scale_arrow = { 0 };
	zest_mesh_t y_scale_arrow = { 0 };
	zest_mesh_t z_scale_arrow = { 0 };
	zest_AddMeshToMesh(&x_scale_arrow, &x_arrow);
	zest_AddMeshToMesh(&y_scale_arrow, &y_arrow);
	zest_AddMeshToMesh(&z_scale_arrow, &z_arrow);
	zest_AddMeshToMesh(&x_arrow, &x_arrow_pointer);
	zest_AddMeshToMesh(&y_arrow, &y_arrow_pointer);
	zest_AddMeshToMesh(&z_arrow, &z_arrow_pointer);
	zest_AddMeshToMesh(&x_scale_arrow, &x_arrow_block);
	zest_AddMeshToMesh(&y_scale_arrow, &y_arrow_block);
	zest_AddMeshToMesh(&z_scale_arrow, &z_arrow_block);
	zest_SetMeshGroupID(&x_arrow, zest_x_rail);
	zest_SetMeshGroupID(&y_arrow, zest_y_rail);
	zest_SetMeshGroupID(&z_arrow, zest_z_rail);
	zest_SetMeshGroupID(&x_plane_mesh, zest_x_plane);
	zest_SetMeshGroupID(&y_plane_mesh, zest_y_plane);
	zest_SetMeshGroupID(&z_plane_mesh, zest_z_plane);
	zest_SetMeshGroupID(&x_scale_arrow, zest_x_rail);
	zest_SetMeshGroupID(&y_scale_arrow, zest_y_rail);
	zest_SetMeshGroupID(&z_scale_arrow, zest_z_rail);
	app->move_widget.parts[zest_x_plane].bb = zest_GetMeshBoundingBox(&x_plane_mesh);
	app->move_widget.parts[zest_y_plane].bb = zest_GetMeshBoundingBox(&y_plane_mesh);
	app->move_widget.parts[zest_z_plane].bb = zest_GetMeshBoundingBox(&z_plane_mesh);
	app->move_widget.parts[zest_x_rail].bb = zest_GetMeshBoundingBox(&x_arrow);
	app->move_widget.parts[zest_y_rail].bb = zest_GetMeshBoundingBox(&y_arrow);
	app->move_widget.parts[zest_z_rail].bb = zest_GetMeshBoundingBox(&z_arrow);
	for(int i =0; i != zest_part_none; ++i) {
		app->move_widget.parts[i].group_id = i;
		app->move_widget.parts[i].transform_matrix = zest_M4(1.f);
	}
	zest_mesh_t move_mesh = { 0 };
	zest_AddMeshToMesh(&move_mesh, &x_arrow);
	zest_AddMeshToMesh(&move_mesh, &y_arrow);
	zest_AddMeshToMesh(&move_mesh, &z_arrow);
	zest_AddMeshToMesh(&move_mesh, &x_plane_mesh);
	zest_AddMeshToMesh(&move_mesh, &z_plane_mesh);
	zest_AddMeshToMesh(&move_mesh, &y_plane_mesh);
	app->move_widget.whole_widget.bb = zest_GetMeshBoundingBox(&move_mesh);
	app->move_widget.whole_widget.transform_matrix = zest_M4(1.f);
	app->move_widget.layer = app->move_widget_layer;
	app->move_widget.type = zest_widget_type_move;
	zest_AddMeshToLayer(app->move_widget_layer, &move_mesh);

	//Scale Widget Setup
	app->scale_widget = {};
	app->scale_widget.parts[zest_x_plane].bb = zest_GetMeshBoundingBox(&x_plane_mesh);
	app->scale_widget.parts[zest_y_plane].bb = zest_GetMeshBoundingBox(&y_plane_mesh);
	app->scale_widget.parts[zest_z_plane].bb = zest_GetMeshBoundingBox(&z_plane_mesh);
	app->scale_widget.parts[zest_x_rail].bb = zest_GetMeshBoundingBox(&x_scale_arrow);
	app->scale_widget.parts[zest_y_rail].bb = zest_GetMeshBoundingBox(&y_scale_arrow);
	app->scale_widget.parts[zest_z_rail].bb = zest_GetMeshBoundingBox(&z_scale_arrow);
	for(int i =0; i != zest_part_none; ++i) {
		app->scale_widget.parts[i].group_id = i;
		app->scale_widget.parts[i].transform_matrix = zest_M4(1.f);
	}
	zest_mesh_t scale_mesh = { 0 };
	zest_AddMeshToMesh(&scale_mesh, &x_plane_mesh);
	zest_AddMeshToMesh(&scale_mesh, &z_plane_mesh);
	zest_AddMeshToMesh(&scale_mesh, &y_plane_mesh);
	zest_AddMeshToMesh(&scale_mesh, &x_scale_arrow);
	zest_AddMeshToMesh(&scale_mesh, &y_scale_arrow);
	zest_AddMeshToMesh(&scale_mesh, &z_scale_arrow);
	zest_AddMeshToLayer(app->scale_widget_layer, &scale_mesh);
	app->scale_widget.whole_widget.bb = zest_GetMeshBoundingBox(&scale_mesh);
	app->scale_widget.whole_widget.transform_matrix = zest_M4(1.f);
	app->scale_widget.layer = app->scale_widget_layer;
	app->scale_widget.type = zest_widget_type_scale;
	SetWidgetPosition(&app->scale_widget, { 2.f, 0, 2.f });

	zest_FreeMesh(&move_mesh);
	zest_FreeMesh(&scale_mesh);
	zest_FreeMesh(&x_arrow);
	zest_FreeMesh(&x_arrow_pointer);
	zest_FreeMesh(&x_arrow_block);
	zest_FreeMesh(&y_arrow);
	zest_FreeMesh(&y_arrow_pointer);
	zest_FreeMesh(&y_arrow_block);
	zest_FreeMesh(&z_arrow);
	zest_FreeMesh(&z_arrow_pointer);
	zest_FreeMesh(&z_arrow_block);
	zest_FreeMesh(&x_plane_mesh);
	zest_FreeMesh(&y_plane_mesh);
	zest_FreeMesh(&z_plane_mesh);
	zest_FreeMesh(&x_scale_arrow);
	zest_FreeMesh(&y_scale_arrow);
	zest_FreeMesh(&z_scale_arrow);

	app->plane_normals[zest_x_plane] = zest_Vec3Set(1.f, 0.f, 0.f);
	app->plane_normals[zest_y_plane] = zest_Vec3Set(0.f, 0.f, 1.f);
	app->plane_normals[zest_z_plane] = zest_Vec3Set(0.f, 1.f, 0.f);
	app->plane_normals[zest_x_rail] = zest_Vec3Set(1.f, 0.f, 0.f);
	app->plane_normals[zest_y_rail] = zest_Vec3Set(1.f, 0.f, 0.f);
	app->plane_normals[zest_z_rail] = zest_Vec3Set(0.f, 0.f, 1.f);

	app->angles = {};

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

	app->cube[0] = { -1.f, 1.f, -1.f };
	app->cube[1] = { 1.f, 1.f, -1.f };
	app->cube[2] = { -1.f, -1.f, -1.f };
	app->cube[3] = { 1.f, -1.f, -1.f };
	app->cube[4] = { -1.f, 1.f, 1.f };
	app->cube[5] = { 1.f, 1.f, 1.f };
	app->cube[6] = { -1.f, -1.f, 1.f };
	app->cube[7] = { 1.f, -1.f, 1.f };

	for (int i = 0; i != 100; ++i) {
		app->particles[i].position = { 0 };
		app->particles[i].velocity = randomVectorInCone({ 1.f, 0.f, 0.f }, zest_Degrees(app->emission_range));
		app->particles[i].age = i * 20.f;
		app->particles[i].speed = app->speed;
		app->particles[i].id = i * 10000;

		app->particles2d[i].position = { 3.f, 0.f, 0.f };
		float direction = randomFloat(-app->emission_range, app->emission_range) + app->emission_direction;
		app->particles2d[i].velocity = { sinf(direction), cosf(direction) };
		app->particles2d[i].age = i * 20.f;
		app->particles2d[i].speed = app->speed;
		app->particles2d[i].id = i * 10000;
	}
}

void HandleWidget(ImGuiApp* app, zest_widget* widget) {
	zest_uniform_buffer_data_t* ubo_ptr = static_cast<zest_uniform_buffer_data_t*>(zest_GetUniformBufferData(ZestRenderer->standard_uniform_buffer));
	zest_vec3 ray_direction = zest_ScreenRay(zest_MouseXf(), zest_MouseYf(), zest_ScreenWidthf(), zest_ScreenHeightf(), &ubo_ptr->proj, &ubo_ptr->view);
	if (!app->picked_widget && !PickWidget(&widget->whole_widget, app->camera.position, ray_direction)) {
		//Early exit if none of the widget is hovered
		widget->hovered_group_id = zest_part_none;
		return;
	}
	if (!app->picked_widget) {
		if (PickWidget(&widget->parts[zest_x_plane], app->camera.position, ray_direction)) {
			widget->hovered_group_id = zest_x_plane;
			app->current_axis = zest_axis_x | zest_axis_y;
		}
		else if (PickWidget(&widget->parts[zest_y_plane], app->camera.position, ray_direction)) {
			widget->hovered_group_id = zest_y_plane;
			app->current_axis = zest_axis_z | zest_axis_y;
		}
		else if (PickWidget(&widget->parts[zest_z_plane], app->camera.position, ray_direction)) {
			widget->hovered_group_id = zest_z_plane;
			app->current_axis = zest_axis_z | zest_axis_x;
		}
		else if (PickWidget(&widget->parts[zest_x_rail], app->camera.position, ray_direction)) {
			widget->hovered_group_id = zest_x_rail;
			app->current_axis = zest_axis_x;
		}
		else if (PickWidget(&widget->parts[zest_y_rail], app->camera.position, ray_direction)) {
			widget->hovered_group_id = zest_y_rail;
			app->current_axis = zest_axis_y;
		}
		else if (PickWidget(&widget->parts[zest_z_rail], app->camera.position, ray_direction)) {
			widget->hovered_group_id = zest_z_rail;
			app->current_axis = zest_axis_z;
		}
		else {
			widget->hovered_group_id = zest_part_none;
		}
	}
	if (widget->hovered_group_id != zest_part_none && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
		zest_vec3 intersection = { 0 };
		float distance = 0.f;
		zest_vec3 plane_normal = app->plane_normals[widget->hovered_group_id];
		float ray_to_plane_normal_dp = zest_DotProduct3(plane_normal, ray_direction);
		plane_normal = zest_ScaleVec3(plane_normal, ray_to_plane_normal_dp < 0 ? 1.f : -1.f);
		if (zest_RayIntersectPlane(app->camera.position, ray_direction, widget->position, plane_normal, &distance, &intersection)) {
			if (!app->picked_widget) {
				app->first_intersection = intersection;
				app->clicked_widget_position = widget->position;
				app->picked_widget_part = &widget->parts[widget->hovered_group_id];
				app->picked_widget = widget;
			}
			if (app->picked_widget_part->group_id <= zest_z_plane) {
				if (app->picked_widget->type == zest_widget_type_move) {
					app->widget_dragged_amount = zest_SubVec3(intersection, app->first_intersection);
					SetWidgetPosition(widget, zest_AddVec3(app->widget_dragged_amount, app->clicked_widget_position));
				}
				else {
					app->widget_dragged_amount = zest_SubVec3(intersection, app->first_intersection);
				}
			}
			else {
				if (ImGui::IsKeyDown(ImGuiKey_Space)) {
					int d = 0;
				}
				switch (app->picked_widget_part->group_id) {
				case zest_x_rail:
					app->widget_dragged_amount = zest_Vec3Set(0.f, 0.f, intersection.z - app->first_intersection.z);
					break;
				case zest_y_rail:
					app->widget_dragged_amount = zest_Vec3Set(0.f, intersection.y - app->first_intersection.y, 0.f);
					break;
				case zest_z_rail:
					app->widget_dragged_amount = zest_Vec3Set(intersection.x - app->first_intersection.x, 0.f, 0.f);
					break;
				}
				if (app->picked_widget->type == zest_widget_type_move) {
					SetWidgetPosition(widget, zest_AddVec3(app->widget_dragged_amount, app->clicked_widget_position));
				}
			}
		}
	}
	if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
		app->picked_widget_part = nullptr;
		app->picked_widget = nullptr;
	}
}

void Draw3dWidgets(ImGuiApp* app) {
	zest_SetInstanceDrawing(app->scale_widget_layer, app->mesh_shader_resources, app->mesh_instance_pipeline);
	zest_SetLayerGlobalPushConstants(app->scale_widget_layer, app->camera.position.x, app->camera.position.y, app->camera.position.z, 1.f);
	zest_SetInstanceDrawing(app->move_widget_layer, app->mesh_shader_resources, app->mesh_instance_pipeline);
	zest_SetLayerGlobalPushConstants(app->move_widget_layer, app->camera.position.x, app->camera.position.y, app->camera.position.z, 1.f);
	zest_SetLayerColor(app->move_widget_layer, 255, 255, 255, 255);

	//X Plane
	zest_vec3 move_widget_position = app->move_widget.position;
	zest_vec3 mesh_rot = zest_Vec3Set(0.f, 0.f, 0.f);
	zest_vec3 cam_to_instance = zest_SubVec3(move_widget_position, app->camera.position);
	float length = zest_LengthVec(cam_to_instance);
	float scale = length / 6.f;

	app->scale_widget_layer->current_instruction.push_constants.parameters1.x = (float)app->scale_widget.hovered_group_id;
	app->move_widget_layer->current_instruction.push_constants.parameters1.x = (float)app->move_widget.hovered_group_id;

	if (app->picked_widget_part && app->picked_widget) {
		app->picked_widget->layer->current_instruction.push_constants.parameters1.x = (float)app->picked_widget_part->group_id;
		zest_Set3DLineDrawing(app->line_layer, app->line_pipeline->shader_resources, app->line_pipeline);
		zest_uniform_buffer_data_t* ubo_ptr = static_cast<zest_uniform_buffer_data_t*>(zest_GetUniformBufferData(ZestRenderer->standard_uniform_buffer));
		zest_axis_flags axis = app->current_axis;
		for (int i = 0; i != 2; ++i) {
			zest_vec3 start = app->clicked_widget_position;
			zest_vec3 end1 = { 0 };
			zest_vec3 end2 = { 0 };
			if (axis & zest_axis_x) {
				end1.z = 1.f; end2.z = -1.f; axis &= ~zest_axis_x;
				zest_SetLayerColor(app->line_layer, 255, 100, 100, 128);
			}
			else if (axis & zest_axis_y) {
				end1.y = 1.f; end2.y = -1.f; axis &= ~zest_axis_y;
				zest_SetLayerColor(app->line_layer, 100, 255, 100, 128);
			}
			else if (axis & zest_axis_z) {
				end1.x = 1.f; end2.x = -1.f; axis &= ~zest_axis_z;
				zest_SetLayerColor(app->line_layer, 100, 100, 255, 128);
			}
			else {
				break;
			}
			float distance;
			zest_vec3 intersection;
			if (zest_RayIntersectPlane(start, end1, app->camera.position, app->camera.front, &distance, &intersection)) {
				start = zest_ScaleVec3(intersection, .999f);
				end1 = zest_AddVec3(zest_ScaleVec3(end2, 1000.f), start);
				zest_Draw3DLine(app->line_layer, &start.x, &end1.x, 3.f);
			}
			else if (zest_RayIntersectPlane(start, end2, app->camera.position, app->camera.front, &distance, &intersection)) {
				start = zest_ScaleVec3(intersection, .999f);
				end2 = zest_AddVec3(zest_ScaleVec3(end1, 1000.f), start);
				zest_Draw3DLine(app->line_layer, &start.x, &end2.x, 3.f);
			}
			else {
				end2 = zest_AddVec3(zest_ScaleVec3(end2, 1000.f), start);
				start = zest_AddVec3(zest_ScaleVec3(end2, -1000.f), start);
				zest_Draw3DLine(app->line_layer, &start.x, &end2.x, 3.f);
			}
		}
		if (app->picked_widget->type == zest_widget_type_scale) {
			zest_vec3 end = zest_AddVec3(app->widget_dragged_amount, app->picked_widget->position);
			zest_Draw3DLine(app->line_layer, &app->picked_widget->position.x, &end.x, 10.f);
		}
	}

	SetWidgetPartScale(&app->move_widget, scale);
	zest_vec3 mesh_scale = { scale,  scale,  scale };
	zest_DrawInstancedMesh(app->move_widget_layer, &move_widget_position.x, &mesh_rot.x, &mesh_scale.x);

	zest_vec3 scale_widget_position = app->scale_widget.position;
	zest_vec3 scale_mesh_rot = zest_Vec3Set(0.f, 0.f, 0.f);
	cam_to_instance = zest_SubVec3(scale_widget_position, app->camera.position);
	length = zest_LengthVec(cam_to_instance);
	scale = length / 6.f;
	mesh_scale = { scale,  scale,  scale };
	SetWidgetPartScale(&app->scale_widget, scale);

	zest_SetLayerColor(app->scale_widget_layer, 255, 255, 255, 255);
	zest_DrawInstancedMesh(app->scale_widget_layer, &scale_widget_position.x, &mesh_rot.x, &mesh_scale.x);
}

zest_vec3 RotateVector(const zest_vec3& vector, const zest_vec3& axis, float angle) {
	float cosTheta = cosf(angle);
	float sinTheta = sinf(angle);
	float dotProduct = zest_DotProduct3(vector, axis);

	zest_vec3 crossProduct = zest_CrossProduct(vector, axis);

	return {
		vector.x * cosTheta + crossProduct.x * sinTheta + axis.x * dotProduct * (1 - cosTheta),
		vector.y * cosTheta + crossProduct.y * sinTheta + axis.y * dotProduct * (1 - cosTheta),
		vector.z * cosTheta + crossProduct.z * sinTheta + axis.z * dotProduct * (1 - cosTheta)
	};
}

void UpdateVelocity3D(particle* particle, float time, float delta_time, float influence) {
	int time_step = (int)(time / 0.25f);
	uint32_t seed = SeedGen({ time_step, particle->id });

	// Generate random values for each dimension
	float point_one_influence = .1f * influence;
	float randomX = (-1.0f + 2.0f * float(SeedGen(seed)) / float(UINT32_MAX));
	float randomY = (-1.0f + 2.0f * float(SeedGen(seed + 1)) / float(UINT32_MAX));
	float randomZ = (-1.0f + 2.0f * float(SeedGen(seed + 2)) / float(UINT32_MAX));
	//zest_vec3 randomDirection = randomVectorInCone2(particle->velocity, 22.5 * influence, seed);
	float randomSpeed = (-1.0f + 2.0f * float(SeedGen(seed + 3)) / float(UINT32_MAX)) * point_one_influence;

	// Create a random direction vector
	//zest_vec3 randomDirection = { randomX, randomY, randomZ };

	// Normalize the random direction vector
	float length = sqrtf(randomX * randomX +
		randomY * randomY +
		randomZ * randomZ);
	zest_vec3 randomDirection{ randomX, randomY, randomZ };
	
	float length_one = 1.f / length;
	randomDirection = zest_ScaleVec3(randomDirection, length_one);

	// Add the random direction to the current velocity
	particle->velocity = zest_AddVec3(particle->velocity, zest_ScaleVec3(randomDirection, point_one_influence));

	// Update speed
	particle->speed += randomSpeed;

	// Normalize the velocity and apply the speed
	length = sqrtf(particle->velocity.x * particle->velocity.x +
		particle->velocity.y * particle->velocity.y +
		particle->velocity.z * particle->velocity.z);
	particle->velocity = zest_NormalizeVec3(particle->velocity);
	zest_vec3 current_velocity = zest_ScaleVec3(particle->velocity, length);

	// Update position
	zest_vec3 position_change = zest_ScaleVec3(current_velocity, delta_time);
	particle->position = zest_AddVec3(particle->position, position_change);
}

void UpdateVelocity2D(particle2d* particle, float time, float delta_time, float influence) {
	int time_step = (int)(time / 0.25f);

	uint32_t seed = SeedGen({time_step, particle->id});

	float randomDirection = (- 1.0f + 2.0f * float(SeedGen(seed)) / float(UINT32_MAX)) * 10.f * influence;
	float randomSpeed = (- 1.0f + 2.0f * float(SeedGen(seed + 1)) / float(UINT32_MAX)) * 0.1f * influence;

	float direction = atan2f(particle->velocity.y, particle->velocity.x);
	direction += zest_Radians(randomDirection);
	particle->speed += randomSpeed;
	particle->velocity = { cosf(direction), sinf(direction) };

	//printf("%i) %i, %f, %f, rs: %f, rd: %f\n", particle->id, seed, particle->speed, direction, randomSpeed, randomDirection);

	// Normalize the velocity and apply the speed
	zest_vec2 position = zest_AddVec2({ particle->position.x, particle->position.y }, zest_ScaleVec2(particle->velocity, particle->speed * delta_time));
	particle->position = { position.x, position.y, 0.f };
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

		ImGui::DragFloat("Pitch", &app->angles.x, 0.1f); if (ImGui::IsItemEdited()) CreatePathPoints(app);
		ImGui::DragFloat("Yaw", &app->angles.y, 0.1f); if (ImGui::IsItemEdited()) CreatePathPoints(app);
		ImGui::DragFloat("Roll", &app->angles.z, 0.1f); if (ImGui::IsItemEdited()) CreatePathPoints(app);

		float pitch = zest_Radians(app->angles.x);
		float yaw = zest_Radians(app->angles.y);
		app->velocity_normal.z = cos(yaw) * cos(pitch);
		app->velocity_normal.y = -sin(pitch);
		app->velocity_normal.x = sin(yaw) * cos(pitch);

		app->velocity_normal = zest_NormalizeVec3(app->velocity_normal);

		Quaternion q = ToQuaternion(zest_Radians(app->angles.x), zest_Radians(app->angles.y), zest_Radians(app->angles.z));
		ImGui::Text("w: %f, x: %f, y: %f, z: %f, length: %f", q.w, q.x, q.y, q.z, q.length());

		zest_vec3 vec = { 0.f, 0.f, 1.f };
		zest_vec3 rot_vec = q.rotate(vec);
		ImGui::Text("Rotated Vec: x: %f, y: %f, z: %f", rot_vec.x, rot_vec.y, rot_vec.z);

		tfxWideArray rx;
		tfxWideArray ry;
		tfxWideArray rz;
		rx.m = _mm_set1_ps(0.f);
		ry.m = _mm_set1_ps(0.f);
		rz.m = _mm_set1_ps(1.f);
		TransformQuaternionVec3(&q, &rx.m, &ry.m, &rz.m);
		ImGui::Text("Rotated Wide Vec: x: %f, y: %f, z: %f", rx.a[0], ry.a[0], rz.a[0]);

		ImGui::DragFloat("Noise Influence", &app->noise_influence, 0.001f, 0.f); 
		ImGui::DragFloat("Noise Frequency", &app->noise_frequency, 0.001f, 0.f);
		ImGui::DragFloat("Particle Speed", &app->speed, 0.001f, 0.f);
		ImGui::DragFloat("Emission Direction", &app->emission_direction, 0.001f, 0.f);
		ImGui::DragFloat("Emission Range", &app->emission_range, 0.001f, 0.f);

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

		HandleWidget(app, &app->move_widget);
		HandleWidget(app, &app->scale_widget);

		ImGui::End();
		ImGui::Render();
		//Let the layer know that it needs to reupload the imgui mesh data to the GPU
		zest_ResetLayer(app->imgui_layer_info.mesh_layer);

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
				app->point.position = zest_AddVec3(app->point.position, zest_ScaleVec3(app->point.direction_normal, 0.1f));
				app->point.position = NearestPointOnEllipsoid(app->ellipse, app->point.position);
				app->point.normal = EllipseSurfaceNormal(app->point.position.x, app->point.position.y, app->point.position.z, app->ellipse.radius.x, app->ellipse.radius.y, app->ellipse.radius.z);
			} else if(ImGui::IsKeyDown(ImGuiKey_S)) {
				zest_vec3 perp = zest_CrossProduct(app->point.normal, app->cross_plane);
				app->point.position = zest_SubVec3(app->point.position, zest_ScaleVec3(perp, 0.1f));
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

	zest_SetMeshDrawing(app->mesh_layer, app->floor_shader_resources, app->mesh_pipeline);
	zest_DrawTexturedPlane(app->mesh_layer, app->floor_image, -500.f, -5.f, -500.f, 1000.f, 1000.f, 50.f, 50.f, 0.f, 0.f);

	zest_SetInstanceDrawing(app->billboard_layer, app->billboard_shader_resources, app->billboard_pipeline);

	zest_Set3DLineDrawing(app->line_layer, app->line_pipeline->shader_resources, app->line_pipeline);
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
			p1 = zest_AddVec3(p1, app->move_widget.position);
			if (start_drawing == true) {
				zest_Draw3DLine(app->line_layer, &p1.x, &p2.x, 3.f);
			}
			start_drawing = true;
			p2 = p1;
		}
	}

	zest_vec3 direction;
	direction.z = cosf(zest_Radians(app->angles.y)) * cosf(zest_Radians(app->angles.x));
	direction.y = -sinf(zest_Radians(app->angles.x));
	direction.x = sinf(zest_Radians(app->angles.y)) * cosf(zest_Radians(app->angles.x));
	zest_vec3 start = { 0.f, 0.f, 0.f };
	for (int i = 0; i != 20; ++i) {
		zest_vec3 rand_vec = randomVectorInCone(direction, 90.f);
		zest_SetLayerColor(app->line_layer, 255, 0, 0, 255);
		end = zest_AddVec3(rand_vec, start);
		zest_vec3 end1 = zest_AddVec3(end, zest_ScaleVec3(rand_vec, 0.1f));
		zest_Draw3DLine(app->line_layer, &end.x, &end1.x, 3.f);
	}

	Quaternion q = ToQuaternion(zest_Radians(app->angles.x), zest_Radians(app->angles.y), 0.f);
	zest_vec3 vec = { 0.f, 0.f, 1.f };
	zest_vec3 rot_vec = q.rotate(vec);
	rot_vec = zest_ScaleVec3(rot_vec, 3.f);

	end = zest_AddVec3(rot_vec, start);
	zest_Draw3DLine(app->line_layer, &start.x, &end.x, 3.f);

	Draw3dWidgets(app);

	Quaternion cube_q = ToQuaternion(zest_Radians(app->angles.x), zest_Radians(app->angles.y), zest_Radians(app->angles.z));

	tfxWideArray rx;
	tfxWideArray ry;
	tfxWideArray rz;
	rx.m = _mm_set1_ps(0.f);
	ry.m = _mm_set1_ps(0.f);
	rz.m = _mm_set1_ps(1.f);
	TransformQuaternionVec3(&cube_q, &rx.m, &ry.m, &rz.m);

	zest_vec3 tform_cube[8];
	tfxWideArray tform_wcube_x[8];
	tfxWideArray tform_wcube_y[8];
	tfxWideArray tform_wcube_z[8];
	for (int i = 0; i != 8; ++i) {
		tfxWideArray xc;
		tfxWideArray yc;
		tfxWideArray zc;
		xc.m = _mm_set1_ps(app->cube[i].x);
		yc.m = _mm_set1_ps(app->cube[i].y);
		zc.m = _mm_set1_ps(app->cube[i].z);
		TransformQuaternionVec3(&cube_q, &xc.m, &yc.m, &zc.m);
		tform_cube[i].x = xc.a[0];
		tform_cube[i].y = yc.a[0];
		tform_cube[i].z = zc.a[0];
	}

	zest_Draw3DLine(app->line_layer, &tform_cube[0].x, &tform_cube[1].x, 3.f);
	zest_Draw3DLine(app->line_layer, &tform_cube[1].x, &tform_cube[3].x, 3.f);
	zest_Draw3DLine(app->line_layer, &tform_cube[3].x, &tform_cube[2].x, 3.f);
	zest_Draw3DLine(app->line_layer, &tform_cube[2].x, &tform_cube[0].x, 3.f);
	zest_Draw3DLine(app->line_layer, &tform_cube[4].x, &tform_cube[5].x, 3.f);
	zest_Draw3DLine(app->line_layer, &tform_cube[5].x, &tform_cube[7].x, 3.f);
	zest_Draw3DLine(app->line_layer, &tform_cube[7].x, &tform_cube[6].x, 3.f);
	zest_Draw3DLine(app->line_layer, &tform_cube[6].x, &tform_cube[4].x, 3.f);
	zest_Draw3DLine(app->line_layer, &tform_cube[6].x, &tform_cube[2].x, 3.f);
	zest_Draw3DLine(app->line_layer, &tform_cube[4].x, &tform_cube[0].x, 3.f);
	zest_Draw3DLine(app->line_layer, &tform_cube[7].x, &tform_cube[3].x, 3.f);
	zest_Draw3DLine(app->line_layer, &tform_cube[5].x, &tform_cube[1].x, 3.f);

	float delta_time = (float)elapsed / 1000000;
	app->time += delta_time;
	for (int i = 0; i != 100; ++i) {
		UpdateVelocity3D(&app->particles[i], app->time, delta_time, app->noise_influence);
		zest_DrawBillboardSimple(app->billboard_layer, app->sprite, &app->particles[i].position.x, 0.f, 0.2f, 0.2f);
		app->particles[i].age += (float)elapsed / 1000;
		if (app->particles[i].age > 4000.f) {
			app->particles[i].speed = app->speed;
			app->particles[i].position = { 0 };
			app->particles[i].velocity = randomVectorInCone({ 1.f, 0.f, 0.f }, zest_Degrees(app->emission_range));
			app->particles[i].age = i * 20.f;
		}

		UpdateVelocity2D(&app->particles2d[i], app->time, delta_time, app->noise_influence);
		//zest_DrawBillboardSimple(app->billboard_layer, app->sprite, &app->particles2d[i].position.x, 0.f, 0.2f, 0.2f);
		app->particles2d[i].age += (float)elapsed / 1000;
		if (app->particles2d[i].age > 4000.f) {
			app->particles2d[i].speed = app->speed;
			app->particles2d[i].position = { 3.f, 0.f, 0.f };
			float direction = randomFloat(-app->emission_range, app->emission_range) + app->emission_direction;
			app->particles2d[i].velocity = { sinf(direction), cosf(direction) };
			app->particles2d[i].age = i * 20.f;
		}
	}

	if (ImGui::IsKeyDown(ImGuiKey_Space)) {
		for (int i = 0; i != 100; ++i) {
			app->particles[i].position = { 0 };
			app->particles[i].velocity = randomVectorInCone({ 1.f, 0.f, 0.f }, 180.f);
			app->particles[i].age = i * 40.f;

			app->particles2d[i].position = { 3.f, 0.f, 0.f };
			float direction = randomFloat(-app->emission_range, app->emission_range) + app->emission_direction;
			app->particles2d[i].velocity = { sinf(direction), cosf(direction) };
			app->particles2d[i].age = i * 40.f;
			app->particles2d[i].speed = app->speed;
		}
	}

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
