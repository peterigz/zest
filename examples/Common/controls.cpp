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
	double mouse_x, mouse_y;
	GLFWwindow *handle = (GLFWwindow *)zest_Window(context);
	glfwGetCursorPos(handle, &mouse_x, &mouse_y);
	double last_mouse_x = mouse->mouse_x;
	double last_mouse_y = mouse->mouse_y;
	mouse->mouse_x = mouse_x;
	mouse->mouse_y = mouse_y;
	mouse->mouse_delta_x = last_mouse_x - mouse->mouse_x;
	mouse->mouse_delta_y = last_mouse_y - mouse->mouse_y;

	bool camera_free_look = false;
	if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
		camera_free_look = true;
		if (glfwRawMouseMotionSupported()) {
			glfwSetInputMode((GLFWwindow *)zest_Window(context), GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
		}
		ZEST__FLAG(ImGui::GetIO().ConfigFlags, ImGuiConfigFlags_NoMouse);
		zest_TurnCamera(camera, (float)mouse->mouse_delta_x, (float)mouse->mouse_delta_y, .05f);
	} else if (glfwRawMouseMotionSupported()) {
		camera_free_look = false;
		ZEST__UNFLAG(ImGui::GetIO().ConfigFlags, ImGuiConfigFlags_NoMouse);
		glfwSetInputMode((GLFWwindow *)zest_Window(context), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	} else {
		camera_free_look = false;
		ZEST__UNFLAG(ImGui::GetIO().ConfigFlags, ImGuiConfigFlags_NoMouse);
	}

	//Restore the mouse when right mouse isn't held down
	if (camera_free_look) {
		glfwSetInputMode((GLFWwindow*)zest_Window(context), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	}
	else {
		glfwSetInputMode((GLFWwindow*)zest_Window(context), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	}
}