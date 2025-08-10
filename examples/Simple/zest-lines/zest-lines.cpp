#include <zest.h>

typedef struct zest_example {
	zest_texture texture;						//A handle to the texture that will contain the bunny image
	zest_image image;							//A handle to image in the texture for the bunny image
	zest_pipeline_template line_pipeline_template;		//The builtin sprite pipeline that will drawing sprites
	zest_layer line_layer;						//The builtin sprite layer that contains the vertex buffer for drawing the sprites
	zest_descriptor_set line_descriptor;		//Hanlde for the billboard descriptor
	zest_shader_resources line_resources;
} zest_example;

void zest_SetShapeDrawing(zest_layer layer, zest_shape_type shape_type, zest_shader_resources shader_resources, zest_pipeline_template pipeline) {
    ZEST_ASSERT_HANDLE(layer);	//Not a valid handle!
    zest_EndInstanceInstructions(layer);
    zest_StartInstanceInstructions(layer);
    layer->current_instruction.pipeline_template = pipeline;
	layer->current_instruction.shader_resources = shader_resources;
	zest_push_constants_t *push = (zest_push_constants_t *)layer->current_instruction.push_constant;
    push->parameters1.x = (float)shape_type;
    layer->current_instruction.draw_mode = (zest_draw_mode)shape_type;
    layer->last_draw_mode = (zest_draw_mode)shape_type;
}

void zest_DrawLine(zest_layer layer, float start_point[2], float end_point[2], float width) {
    ZEST_ASSERT_HANDLE(layer);	//Not a valid handle!
    ZEST_ASSERT(layer->current_instruction.draw_mode == zest_draw_mode_line_instance || layer->current_instruction.draw_mode == zest_draw_mode_dashed_line);    //Call zest_StartSpriteDrawing before calling this function

    zest_shape_instance_t* line = (zest_shape_instance_t*)layer->memory_refs[ZEST_FIF].instance_ptr;

    line->rect.x = start_point[0];
    line->rect.y = start_point[1];
    line->rect.z = end_point[0];
    line->rect.w = end_point[1];
    line->parameters.x = width;
    line->parameters.x = width;
    line->start_color = layer->current_color;
    line->end_color = layer->current_color;
    layer->current_instruction.total_instances++;

    zest_NextInstance(layer);
}

void zest_DrawRect(zest_layer layer, float top_left[2], float width, float height) {
    ZEST_ASSERT_HANDLE(layer);	//Not a valid handle!
    ZEST_ASSERT(layer->current_instruction.draw_mode == zest_draw_mode_rect_instance);    //Call zest_StartSpriteDrawing before calling this function

    zest_shape_instance_t* line = (zest_shape_instance_t*)layer->memory_refs[ZEST_FIF].instance_ptr;

    line->rect.x = top_left[0];
    line->rect.y = top_left[1];
    line->rect.z = top_left[0] + width;
    line->rect.w = top_left[1] + height;
    line->start_color = layer->current_color;
    line->end_color = layer->current_color;
    layer->current_instruction.total_instances++;

    zest_NextInstance(layer);
}

void InitExample(zest_example *example) {
	//Create a new texture for storing the bunny image
	example->texture = zest_CreateTexture("Example Texture", zest_texture_storage_type_bank, zest_texture_flag_use_filtering, zest_texture_format_rgba_unorm, 10);
	//Load in the bunny image from file and add it to the texture
	example->image = zest_AddTextureImageFile(example->texture, "examples/assets/wabbit_alpha.png");
	//Process the texture which will create the resources on the GPU for sampling from the bunny image texture
	zest_ProcessTextureImages(example->texture);

	//SDF lines 2d
	example->line_pipeline_template = zest_CopyPipelineTemplate("pipeline_line_instance", zest_PipelineTemplate("pipeline_2d_sprites"));
	zest_ClearVertexInputBindingDescriptions(example->line_pipeline_template);
	zest_ClearVertexAttributeDescriptions(example->line_pipeline_template);
	zest_AddVertexInputBindingDescription(example->line_pipeline_template, 0, sizeof(zest_shape_instance_t), VK_VERTEX_INPUT_RATE_INSTANCE);

	zest_AddVertexAttribute(example->line_pipeline_template, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(zest_shape_instance_t, rect));        // Location 0: Start Position
	zest_AddVertexAttribute(example->line_pipeline_template, 1, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(zest_shape_instance_t, parameters));  // Location 1: End Position
	zest_AddVertexAttribute(example->line_pipeline_template, 2, VK_FORMAT_R8G8B8A8_UNORM, offsetof(zest_shape_instance_t, start_color));      // Location 2: Start Color

	zest_SetText(&example->line_pipeline_template->vertShaderFile, "shape_vert.spv");
	zest_SetText(&example->line_pipeline_template->fragShaderFile, "shape_frag.spv");
	zest_ClearPipelineDescriptorLayouts(example->line_pipeline_template);
	zest_AddPipelineDescriptorLayout(example->line_pipeline_template, zest_GetDefaultUniformBufferLayout()->vk_layout);
	zest_EndPipelineTemplate(example->line_pipeline_template);
	example->line_pipeline_template->colorBlendAttachment = zest_PreMultiplyBlendState();
	example->line_pipeline_template->depthStencil.depthWriteEnable = VK_FALSE;
	ZEST_APPEND_LOG(ZestDevice->log_path.str, "SDF Lines pipeline");

	example->line_layer = zest_CreateInstanceLayer("Lines", sizeof(zest_line_instance_t));

	example->line_resources = zest_CreateShaderResources();
	zest_AddUniformBufferToResources(example->line_resources, ZestRenderer->uniform_buffer);
}

void test_update_callback(zest_microsecs elapsed, void *user_data) {
	//Get the example struct from the user data we set with zest_SetUserData
	zest_example *example = (zest_example*)user_data;
	//Update the builtin 2d uniform buffer. 
	zest_Update2dUniformBuffer();

	zest_SetShapeDrawing(example->line_layer, zest_shape_dashed_line, example->line_resources, example->line_pipeline_template);
	zest_SetLayerColor(example->line_layer, 255, 255, 255, 100);
	zest_vec2 start = zest_Vec2Set(20.f, 20.f);
	zest_vec2 end = zest_Vec2Set((float)ZestApp->mouse_x, (float)ZestApp->mouse_y);
	zest_DrawLine(example->line_layer, &start.x, &end.x, 10.f);

	zest_SetShapeDrawing(example->line_layer, zest_shape_rect, example->line_resources, example->line_pipeline_template);
	zest_vec2 top_left = zest_Vec2Set((float)ZestApp->mouse_x, (float)ZestApp->mouse_y);
	zest_DrawRect(example->line_layer, &top_left.x, 30.f, 50.f);

	//Create the render graph
	if (zest_BeginRenderToScreen("Lines Render Graph")) {
		VkClearColorValue clear_color = { {0.0f, 0.1f, 0.2f, 1.0f} };

		//Resources
		zest_resource_node swapchain_output_resource = zest__import_swapchain_resource("Swapchain Output");
		zest_resource_node line_layer_resources = zest_AddTransientLayerResource("Line layer", example->line_layer, false);

		//---------------------------------Transfer Pass----------------------------------------------------
		zest_pass_node upload_line_data = zest_AddTransferPassNode("Upload Line Data");
		//outputs
		zest_ConnectTransferBufferOutput(upload_line_data, line_layer_resources);
		//tasks
		zest_AddPassTask(upload_line_data, zest_UploadInstanceLayerData, example->line_layer);
		//--------------------------------------------------------------------------------------------------

		//Add passes
		//---------------------------------Render Pass------------------------------------------------------
		zest_pass_node graphics_pass = zest_AddRenderPassNode("Graphics Pass");
		//inputs
		zest_ConnectVertexBufferInput(graphics_pass, line_layer_resources);
		//outputs
		zest_ConnectSwapChainOutput(graphics_pass, swapchain_output_resource, clear_color);
		//tasks
		zest_AddPassTask(graphics_pass, zest_DrawInstanceLayer, example->line_layer);
		//--------------------------------------------------------------------------------------------------

		//Compile and execute the render graph
		zest_render_graph render_graph = zest_EndRenderGraph();

		//Print the render graph
		static bool print_render_graph = true;
		if (print_render_graph) {
			zest_PrintCompiledRenderGraph(render_graph);
			print_render_graph = false;
		}
	}
}

#if defined(_WIN32)
// Windows entry point
//int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow)
int main(void) 
{
	zest_example example = { 0 };

	zest_create_info_t create_info = zest_CreateInfoWithValidationLayers(0);
	ZEST__UNFLAG(create_info.flags, zest_init_flag_enable_vsync);
	ZEST__FLAG(create_info.flags, zest_init_flag_use_depth_buffer);
	ZEST__FLAG(create_info.flags, zest_init_flag_log_validation_errors_to_console);
	create_info.log_path = "./";

	zest_Initialise(&create_info);
	zest_LogFPSToConsole(1);
	zest_SetUserData(&example);
	zest_SetUserUpdateCallback(test_update_callback);

	InitExample(&example);

	zest_Start();

	return 0;
}
#else
int main(void) {
	zest_example example = { 0 };

	zest_create_info_t create_info = zest_CreateInfo();
	ZEST__UNFLAG(create_info.flags, zest_init_flag_enable_vsync);

	zest_Initialise(&create_info);
	zest_LogFPSToConsole(1);
	zest_SetUserData(&example);
	zest_SetUserUpdateCallback(test_update_callback);

	InitExample(&example);

	zest_Start();

	return 0;
}
#endif
