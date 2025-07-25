#pragma once

#include <zest.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui/imgui.h"
#include <imgui/misc/freetype/imgui_freetype.h>
#include <imgui/backends/imgui_impl_glfw.h>

zest_imgui zest_imgui_Initialise();
void zest_imgui_Shutdown();

