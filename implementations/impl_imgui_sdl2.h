#pragma once

#include <zest.h>
#include "imgui/imgui.h"
#include <imgui/misc/freetype/imgui_freetype.h>
#include <imgui/backends/imgui_impl_sdl2.h>

ZEST_API void zest_imgui_InitialiseForSDL();
ZEST_API void zest_imgui_ShutdownSDL();
