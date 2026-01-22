void UpdateCameraPosition(zest_timer_t *timer, zest_vec3 *new_camera_position, zest_vec3 *old_camera_position, zest_camera_t *camera, float distance_per_second) {
	float speed = distance_per_second * (float)zest_TimerUpdateTime(timer);
	*old_camera_position = camera->position;
	if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
		ImGui::SetWindowFocus(nullptr);

		if (ImGui::IsKeyDown(ImGuiKey_W)) {
			*new_camera_position = zest_AddVec3(*new_camera_position, zest_ScaleVec3(camera->front, speed));
		}
		if (ImGui::IsKeyDown(ImGuiKey_S)) {
			*new_camera_position = zest_SubVec3(*new_camera_position, zest_ScaleVec3(camera->front, speed));
		}
		if (ImGui::IsKeyDown(ImGuiKey_A)) {
			zest_vec3 cross = zest_NormalizeVec3(zest_CrossProduct(camera->front, camera->up));
			*new_camera_position = zest_SubVec3(*new_camera_position, zest_ScaleVec3(cross, speed));
		}
		if (ImGui::IsKeyDown(ImGuiKey_D)) {
			zest_vec3 cross = zest_NormalizeVec3(zest_CrossProduct(camera->front, camera->up));
			*new_camera_position = zest_AddVec3(*new_camera_position, zest_ScaleVec3(cross, speed));
		}
	}
}

void UpdateMouse(zest_context context, mouse_t *mouse, zest_camera_t *camera) {
	int mouse_x, mouse_y;
	SDL_GetMouseState(&mouse_x, &mouse_y);
	double last_mouse_x = mouse->mouse_x;
	double last_mouse_y = mouse->mouse_y;
	mouse->mouse_x = (double)mouse_x;
	mouse->mouse_y = (double)mouse_y;
	mouse->mouse_delta_x = last_mouse_x - mouse->mouse_x;
	mouse->mouse_delta_y = last_mouse_y - mouse->mouse_y;

	bool camera_free_look = false;
	int rel_x, rel_y;
	SDL_GetRelativeMouseState(&rel_x, &rel_y);
	if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
		camera_free_look = true;
		SDL_SetRelativeMouseMode(SDL_TRUE);
		ZEST__FLAG(ImGui::GetIO().ConfigFlags, ImGuiConfigFlags_NoMouse);
		// Use relative mouse motion for smoother camera control
		zest_TurnCamera(camera, (float)-rel_x, (float)-rel_y, .05f);
	} else {
		camera_free_look = false;
		ZEST__UNFLAG(ImGui::GetIO().ConfigFlags, ImGuiConfigFlags_NoMouse);
		SDL_SetRelativeMouseMode(SDL_FALSE);
	}
}