#include <zest.h>

typedef struct zest_example {
	zest_font font;
	zest_layer font_layer;
	zest_draw_routine draw_routine;
	zest_render_graph render_graph;
} zest_example;

void InitExample(zest_example *example) {
	example->font_layer = zest_CreateFontLayer("Example fonts");

	//Load a font and store the handle. MSDF fonts are in the zft format which you can create using zest-msdf-font-maker
	example->font = zest_LoadMSDFFont("examples/assets/KaushanScript-Regular.zft");

	example->render_graph = zest_NewRenderGraph("Fonts Example Render Graph", 0, false);

	ZEST_APPEND_LOG(ZestDevice->log_path.str, "Font pipeline");
}

void UploadFontData(VkCommandBuffer command_buffer, const zest_render_graph_context_t *context, void *user_data) {
	zest_example *example = (zest_example*)user_data;
    zest_EndInstanceInstructions(example->font_layer);

	zest_buffer staging_buffer = example->font_layer->memory_refs.staging_instance_data;
	zest_buffer device_buffer = zest_GetPassOutputResource(context->pass_node, "Font Buffer")->storage_buffer;

	zest_buffer_uploader_t instance_upload = { 0, staging_buffer, device_buffer, 0 };

	if (staging_buffer->memory_in_use) {
		zest_AddCopyCommand(&instance_upload, staging_buffer, device_buffer, 0);
	}

	zest_uint vertex_size = zest_vec_size(instance_upload.buffer_copies);

	zest_UploadBuffer(&instance_upload, command_buffer);
}

void RecordFonts(VkCommandBuffer command_buffer, const zest_render_graph_context_t *context, void *user_data) {
	//Grab the app object from the user_data that we set in the render graph when adding this function callback 
	zest_example *app = (zest_example *)user_data;
	zest_layer layer = app->font_layer;
	zest_buffer device_buffer = zest_GetPassInputResource(context->pass_node, "Font Buffer")->storage_buffer;
	VkDeviceSize instance_data_offsets[] = { device_buffer->memory_offset };
	vkCmdBindVertexBuffers(command_buffer, 0, 1, &device_buffer->memory_pool->buffer, instance_data_offsets);
    bool has_instruction_view_port = false;
    for (zest_foreach_i(layer->draw_instructions[ZEST_FIF])) {
        zest_layer_instruction_t* current = &layer->draw_instructions[ZEST_FIF][i];

        if (current->draw_mode == zest_draw_mode_viewport) {
            vkCmdSetViewport(command_buffer, 0, 1, &current->viewport);
            vkCmdSetScissor(command_buffer, 0, 1, &current->scissor);
            has_instruction_view_port = true;
            continue;
        } else if(!has_instruction_view_port) {
            vkCmdSetViewport(command_buffer, 0, 1, &layer->viewport);
            vkCmdSetScissor(command_buffer, 0, 1, &layer->scissor);
        }

		VkDescriptorSet sets[] = {
			app->font->shader_resources->sets[ZEST_FIF][0]->vk_descriptor_set,
			app->font->shader_resources->sets[ZEST_FIF][1]->vk_descriptor_set
		};

        zest_pipeline pipeline = zest_PipelineWithTemplate(current->pipeline_template, context->render_pass);
        zest_BindPipeline(command_buffer, pipeline, sets, 2);

        //The only reason I do this is because of some strange alignment issues on intel macs only. I haven't fully gotten to the bottom of it
        //but this seems to work for now
        memcpy(&current->push_constants.global.x, &layer->global_push_values.x, sizeof(zest_vec4));

		vkCmdPushConstants(
			command_buffer,
			pipeline->pipeline_layout,
			zest_PipelinePushConstantStageFlags(pipeline, 0),
			zest_PipelinePushConstantOffset(pipeline, 0),
			zest_PipelinePushConstantSize(pipeline, 0),
			&current->push_constants);

        vkCmdDraw(command_buffer, 6, current->total_instances, 0, current->start_index);

        zest_vec_clear(layer->draw_sets);
    }
	zest_ResetInstanceLayerDrawing(layer);
}

void UpdateCallback(zest_microsecs elapsed, void *user_data) {
	//Get the example struct from the user data we set with zest_SetUserData
	zest_example *example = (zest_example*)user_data;
	//Update the uniform buffer.
	zest_Update2dUniformBuffer();

	//Set the instensity of the font
	example->font_layer->intensity = 1.f;
	//Let the layer know that we want to draw text with the following font
	zest_SetMSDFFontDrawing(example->font_layer, example->font);
	//Draw a paragraph of text
	zest_DrawMSDFParagraph(example->font_layer, "Zest fonts drawn using MSDF!\nHere are some numbers 12345\nLorem ipsum and all that\nHello Sailer!\n", 0.f, 0.f, 0.f, 0.f, 50.f, 0.f, 1.f);
	//Draw a single line of text
	zest_DrawMSDFText(example->font_layer, "(This should be centered)", zest_ScreenWidth() * .5f, zest_ScreenHeightf() * .5f, .5f, .5f, 50.f, 0.f);

	if (zest_BeginRenderToScreen(example->render_graph)) {
		VkClearColorValue clear_color = { {0.0f, 0.1f, 0.2f, 1.0f} };
		//Import the swap chain into the render pass
		zest_resource_node swapchain_output_resource = zest_ImportSwapChainResource(example->render_graph, "Swapchain Output");
		zest_resource_node font_layer_resources = zest_AddFontLayerResources(example->render_graph, "Font Buffer", example->font_layer);
		zest_pass_node font_pass = zest_AddGraphicPassNode(example->render_graph, "Draw Fonts");
		zest_pass_node upload_font_data = zest_AddTransferPassNode(example->render_graph, "Upload Font Data");
        zest_AddPassTask(upload_font_data, UploadFontData, example);
		zest_AddPassTransferDstBuffer(upload_font_data, font_layer_resources);
		zest_AddPassVertexBufferInput(font_pass, font_layer_resources);
		zest_AddPassTask(font_pass, RecordFonts, example);
		zest_AddPassSwapChainOutput(font_pass, swapchain_output_resource, clear_color);

	}
	zest_EndRenderGraph(example->render_graph);
	//zest_PrintCompiledRenderGraph(example->render_graph);
	zest_ExecuteRenderGraph(example->render_graph);

}

#if defined(_WIN32)
// Windows entry point
//int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow) {
int main(void) {
	zest_create_info_t create_info = zest_CreateInfoWithValidationLayers();
	create_info.log_path = "./";
	ZEST__FLAG(create_info.flags, zest_init_flag_log_validation_errors_to_console);
	zest_example example;

	zest_Initialise(&create_info);
	zest_SetUserData(&example);
	zest_SetUserUpdateCallback(UpdateCallback);

	InitExample(&example);

	zest_Start();

	return 0;
}
#else
int main(void) {

	zest_create_info_t create_info = zest_CreateInfo();

	zest_example example;
    
	zest_Initialise(&create_info);
	zest_SetUserData(&example);
	zest_SetUserUpdateCallback(UpdateCallback);

	InitExample(&example);

	zest_Start();

	return 0;
}
#endif
