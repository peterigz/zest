#include "impl_imgui_glfw.h"
#include "impl_imgui.h"
#include "imgui_internal.h"

void zest_imgui_InitialiseForGLFW(zest_context context) {
	zest_imgui_Initialise(context);
    ImGui_ImplGlfw_InitForVulkan((GLFWwindow *)zest_Window(context), true);
}

void zest_imgui_ShutdownGLFW() {
	ImGui_ImplGlfw_Shutdown();
	zest_imgui_Shutdown();
}

