#pragma once

#include <zest.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui/imgui.h"
#include <imgui/misc/freetype/imgui_freetype.h>
#include <imgui/backends/imgui_impl_glfw.h>

void zest_imgui_InitialiseForGLFW(zest_context context);
void zest_imgui_ShutdownGLFW();



