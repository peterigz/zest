#pragma once

#include <zest.h>
#include "implementations/impl_glfw.h"
#include "implementations/impl_imgui_glfw.h"
#include "imgui/imgui.h"
#include <imgui/misc/freetype/imgui_freetype.h>
#include <imgui/backends/imgui_impl_glfw.h>

const float pi = 3.14159265359f;
const float two_pi = 2.f * 3.14159265359f;

typedef zest_uint zest_axis_flags;
enum zest_axis_bits {
	zest_Axis_none = 0,
	zest_axis_x		= 1 << 0,
	zest_axis_y		= 1 << 1,
	zest_axis_z		= 1 << 2
};

enum zest_widget_type {
	zest_widget_type_move,
	zest_widget_type_scale,
};

enum zest_widget_part_index {
	zest_x_plane,
	zest_y_plane,
	zest_z_plane,
	zest_x_rail,
	zest_y_rail,
	zest_z_rail,
	zest_part_none
};

struct ellipsoid {
	zest_vec3 position;
	zest_vec3 radius;
};

struct cylinder_t {
	zest_vec3 position;
	zest_vec2 radius;
	float height;
};

struct surface_point {
	zest_vec3 position;
	zest_vec3 normal;
	zest_vec2 uv;
	float direction;
	zest_vec3 direction_normal;
};

struct zest_widget_part {
	zest_bounding_box_t bb;
	zest_matrix4 transform_matrix;
	zest_uint group_id;
	float scale;
};

struct zest_widget {
	zest_vec3 position;
	zest_layer layer;
	zest_uint hovered_group_id;
	zest_widget_type type;
	zest_widget_part parts[6];
	zest_widget_part whole_widget;
};

struct ImGuiApp {
	zest_imgui_layer_info imgui_layer_info;
	zest_index imgui_draw_routine_index;
	zest_texture imgui_font_texture;
	zest_texture floor_texture;
	zest_texture sprite_texture;
	zest_image floor_image;
	zest_image sprite;
	zest_timer timer;
	zest_layer mesh_layer;
	zest_layer billboard_layer;
	zest_layer lines_2d;
	zest_layer line_layer;
	zest_pipeline mesh_pipeline;
	zest_pipeline line_pipeline;
	zest_pipeline line_2d_pipeline;
	zest_pipeline billboard_pipeline;

	zest_pipeline mesh_instance_pipeline;
	zest_layer move_widget_layer;
	zest_layer scale_widget_layer;
	zest_mesh_t mesh;
	zest_widget move_widget;
	zest_widget scale_widget;
	zest_widget_part *picked_widget_part = nullptr;
	zest_widget *picked_widget = nullptr;
	zest_vec3 first_intersection;
	zest_vec3 clicked_widget_position;
	zest_uniform_buffer uniform_buffer_3d;
	zest_descriptor_set_layout descriptor_layout;
	zest_descriptor_set_t descriptor_set;
	zest_vec3 plane_normals[6];
	zest_axis_flags current_axis;

	zest_camera_t camera;
	ellipsoid ellipse;
	cylinder_t cylinder;
	surface_point points[1000];
	surface_point point;
	bool repoint = true;
	bool orthagonal = false;
	zest_vec3 cross_plane;

	float height_increment;
	float angle_increment;
	float radius;
	float spline_points;
};

void InitImGuiApp(ImGuiApp *app);

void DarkStyle2() {
	ImGui::GetStyle().FrameRounding = 4.0f;
	ImGui::GetStyle().GrabRounding = 4.0f;

	ImVec4* colors = ImGui::GetStyle().Colors;
	colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
	colors[ImGuiCol_TextDisabled] = ImVec4(1.00f, 1.00f, 1.00f, 0.26f);
	colors[ImGuiCol_WindowBg] = ImVec4(0.15f, 0.15f, 0.15f, 0.95f);
	colors[ImGuiCol_ChildBg] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
	colors[ImGuiCol_PopupBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
	colors[ImGuiCol_Border] = ImVec4(1.00f, 1.00f, 1.00f, 0.07f);
	colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.32f);
	colors[ImGuiCol_FrameBg] = ImVec4(0.11f, 0.11f, 0.11f, 1.00f);
	colors[ImGuiCol_FrameBgHovered] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
	colors[ImGuiCol_FrameBgActive] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
	colors[ImGuiCol_TitleBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
	colors[ImGuiCol_TitleBgActive] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
	colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
	colors[ImGuiCol_MenuBarBg] = ImVec4(0.15f, 0.15f, 0.15f, 0.95f);
	colors[ImGuiCol_ScrollbarBg] = ImVec4(0.02f, 0.02f, 0.02f, 0.34f);
	colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.86f, 0.31f, 0.02f, 1.00f);
	colors[ImGuiCol_CheckMark] = ImVec4(0.91f, 0.35f, 0.05f, 1.00f);
	colors[ImGuiCol_SliderGrab] = ImVec4(0.86f, 0.31f, 0.02f, 0.53f);
	colors[ImGuiCol_SliderGrabActive] = ImVec4(0.86f, 0.31f, 0.02f, 1.00f);
	colors[ImGuiCol_Button] = ImVec4(0.5f, 0.5f, 0.5f, .25f);
	colors[ImGuiCol_ButtonHovered] = ImVec4(0.86f, 0.31f, 0.02f, 0.53f);
	colors[ImGuiCol_ButtonActive] = ImVec4(0.86f, 0.31f, 0.02f, 1.00f);
	colors[ImGuiCol_Header] = ImVec4(0.00f, 0.00f, 0.00f, 0.36f);
	colors[ImGuiCol_HeaderHovered] = ImVec4(0.86f, 0.31f, 0.02f, 0.53f);
	colors[ImGuiCol_HeaderActive] = ImVec4(0.86f, 0.31f, 0.02f, 0.53f);
	colors[ImGuiCol_Separator] = ImVec4(0.00f, 0.00f, 0.00f, 0.22f);
	colors[ImGuiCol_SeparatorHovered] = ImVec4(0.86f, 0.31f, 0.02f, 0.53f);
	colors[ImGuiCol_SeparatorActive] = ImVec4(0.86f, 0.31f, 0.02f, 1.00f);
	colors[ImGuiCol_ResizeGrip] = ImVec4(0.86f, 0.31f, 0.02f, 0.53f);
	colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.86f, 0.31f, 0.02f, 0.53f);
	colors[ImGuiCol_ResizeGripActive] = ImVec4(0.86f, 0.31f, 0.02f, 1.00f);
	colors[ImGuiCol_Tab] = ImVec4(0.17f, 0.17f, 0.17f, 0.00f);
	colors[ImGuiCol_TabHovered] = ImVec4(0.86f, 0.31f, 0.02f, 0.80f);
	colors[ImGuiCol_TabActive] = ImVec4(0.86f, 0.31f, 0.02f, 1.00f);
	colors[ImGuiCol_TabUnfocused] = ImVec4(0.86f, 0.31f, 0.02f, 0.00f);
	colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.86f, 0.31f, 0.02f, 0.68f);
	colors[ImGuiCol_DockingPreview] = ImVec4(0.86f, 0.31f, 0.02f, 1.00f);
	colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.00f);
	colors[ImGuiCol_PlotLines] = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
	colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
	colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
	colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
	colors[ImGuiCol_TableHeaderBg] = ImVec4(0.08f, 0.08f, 0.08f, 0.59f);
	colors[ImGuiCol_TableBorderStrong] = ImVec4(0.00f, 0.00f, 0.00f, 0.2f);
	colors[ImGuiCol_TableBorderLight] = ImVec4(0.00f, 0.00f, 0.00f, 0.1f);
	colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.00f, 0.00f, 0.00f, 0.11f);
	colors[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
	colors[ImGuiCol_DragDropTarget] = ImVec4(0.86f, 0.31f, 0.02f, 1.00f);
	colors[ImGuiCol_NavHighlight] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
	colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
	colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);

	ImGuiStyle &style = ImGui::GetStyle();
	style.WindowPadding.x = 10.f;
	style.WindowPadding.y = 10.f;
	style.FramePadding.x = 6.f;
	style.FramePadding.y = 4.f;
	style.ItemSpacing.x = 10.f;
	style.ItemSpacing.y = 6.f;
	style.ItemInnerSpacing.x = 5.f;
	style.ItemInnerSpacing.y = 5.f;
	style.WindowRounding = 0.f;
	style.FrameRounding = 4.f;
	style.ScrollbarRounding = 1.f;
	style.GrabRounding = 1.f;
	style.TabRounding = 4.f;
	style.IndentSpacing = 20.f;
	style.FrameBorderSize = 1.0f;

	style.ChildRounding = 0.f;
	style.PopupRounding = 0.f;
	style.CellPadding.x = 4.f;
	style.CellPadding.y = 2.f;
	style.TouchExtraPadding.x = 0.f;
	style.TouchExtraPadding.y = 0.f;
	style.ColumnsMinSpacing = 6.f;
	style.ScrollbarSize = 14.f;
	style.ScrollbarRounding = 1.f;
	style.GrabMinSize = 10.f;
	style.TabMinWidthForCloseButton = 0.f;
	style.DisplayWindowPadding.x = 19.f;
	style.DisplayWindowPadding.y = 19.f;
	style.DisplaySafeAreaPadding.x = 3.f;
	style.DisplaySafeAreaPadding.y = 3.f;
	style.MouseCursorScale = 1.f;
	style.WindowMinSize.x = 32.f;
	style.WindowMinSize.y = 32.f;
	style.ChildBorderSize = 1.f;

}
