#include "zest-msdf-fonts.h"

//-- Fonts
void zest_LoadMSDFFont(const char *filename, zest_font_t *font) {
    font->magic = zest_INIT_MAGIC(0);

    char *font_data = zest_ReadEntireFile(filename, 0);
    ZEST_ASSERT(font_data);            //File not found
    zest_vec_resize(font->characters, 256);
    font->characters['\n'].flags = zest_character_flag_new_line;

    zest_font_character_t c;
    zest_uint character_count = 0;
    zest_uint file_version = 0;

    zest_uint position = 0;
    char magic_number[6];
    memcpy(magic_number, font_data + position, 6);
    position += 6;
    ZEST_ASSERT(strcmp((const char *)magic_number, "!TSEZ") == 0);        //Not a valid ztf file

    memcpy(&character_count, font_data + position, sizeof(zest_uint));
    position += sizeof(zest_uint);
    memcpy(&file_version, font_data + position, sizeof(zest_uint));
    position += sizeof(zest_uint);
    memcpy(&font->pixel_range, font_data + position, sizeof(float));
    position += sizeof(float);
    memcpy(&font->miter_limit, font_data + position, sizeof(float));
    position += sizeof(float);
    memcpy(&font->padding, font_data + position, sizeof(float));
    position += sizeof(float);

    for (zest_uint i = 0; i != character_count; ++i) {
        memcpy(&c, font_data + position, sizeof(zest_font_character_t));
        position += sizeof(zest_font_character_t);

        font->max_yoffset = ZEST__MAX(fabsf(c.yoffset), font->max_yoffset);

        const char key = c.character[0];
        c.uv_packed = zest_Pack16bit4SNorm(c.uv.x, c.uv.y, c.uv.z, c.uv.w);
        font->characters[key] = c;
    }

    memcpy(&font->font_size, font_data + position, sizeof(float));
    position += sizeof(float);

    zest_uint image_size;

    memcpy(&image_size, font_data + position, sizeof(zest_uint));
    position += sizeof(zest_uint);
    zest_byte *image_data = 0;
    zest_vec_resize(image_data, image_size);
    size_t buffer_size;
    memcpy(&buffer_size, font_data + position, sizeof(size_t));
    position += sizeof(size_t);
    memcpy(image_data, font_data + position, buffer_size);

    font->texture = zest_CreateTextureSingle(filename, zest_format_r8g8b8a8_unorm);

    zest_bitmap bitmap = zest__get_texture_single_bitmap(texture);

    stbi_set_flip_vertically_on_load(1);
    zest_LoadBitmapImageMemory(bitmap, image_data, image_size, 0);

    zest_atlas_region_t *image = &texture->texture;
    image->width = bitmap->meta.width;
    image->height = bitmap->meta.height;

    zest_vec_free(image_data);
    zest_vec_free(font_data);

    zest_SetText(&font->name, filename);

    zest__setup_font_texture(font);
    stbi_set_flip_vertically_on_load(0);

    return font;
}

void zest_DrawFonts(VkCommandBuffer command_buffer, const zest_frame_graph_context_t *context, void *user_data) {
    //Grab the app object from the user_data that we set in the frame graph when adding this function callback 
    zest_layer_handle layer = (zest_layer_handle)user_data;
    ZEST_ASSERT_HANDLE(layer);       //Not a valid layer. Make sure that you pass in the font layer to the zest_AddPassTask function

    zest_buffer device_buffer = layer->vertex_buffer_node->storage_buffer;
    if (!device_buffer) {
        return;
    }
    VkDeviceSize instance_data_offsets[] = { device_buffer->buffer_offset };
    vkCmdBindVertexBuffers(command_buffer, 0, 1, &device_buffer->vk_buffer, instance_data_offsets);
    bool has_instruction_view_port = false;

    VkDescriptorSet sets[] = {
        zest_vk_GetGlobalUniformBufferDescriptorSet(),
        zest_vk_GetGlobalBindlessSet()
    };

    zest_vec_foreach(i, layer->draw_instructions[layer->fif]) {
        zest_layer_instruction_t *current = &layer->draw_instructions[layer->fif][i];

        if (current->draw_mode == zest_draw_mode_viewport) {
            vkCmdSetViewport(command_buffer, 0, 1, &current->viewport);
            vkCmdSetScissor(command_buffer, 0, 1, &current->scissor);
            has_instruction_view_port = true;
            continue;
        } else if (!has_instruction_view_port) {
            vkCmdSetViewport(command_buffer, 0, 1, &layer->viewport);
            vkCmdSetScissor(command_buffer, 0, 1, &layer->scissor);
        }

        zest_pipeline pipeline = zest_PipelineWithTemplate(current->pipeline_template, context->render_pass);
        if (pipeline) {
            zest_cmd_BindPipeline(command_buffer, pipeline, sets, 2);
        } else {
            continue;
        }

        vkCmdPushConstants(
            command_buffer,
            pipeline->pipeline_layout,
            zest_PipelinePushConstantStageFlags(pipeline, 0),
            zest_PipelinePushConstantOffset(pipeline, 0),
            zest_PipelinePushConstantSize(pipeline, 0),
            &current->push_constant);

        vkCmdDraw(command_buffer, 6, current->total_instances, 0, current->start_index);
    }
    zest_ResetInstanceLayerDrawing(layer);
}

void zest_FreeFont(zest_font_t *font) {
    ZEST_ASSERT_HANDLE(font);
    zest_vec_push(ZestRenderer->deferred_resource_freeing_list.resources[ZEST_FIF], font);
}

void zest__setup_font_texture(zest_font_t *font) {
    ZEST_ASSERT_HANDLE(font);	//Not a valid handle!
    zest_texture texture = (zest_texture)zest__get_store_resource(&ZestRenderer->textures, font->texture.value);
    ZEST_ASSERT_HANDLE(texture);    //Not a valid texture resource

    if (ZEST__FLAGGED(texture->flags, zest_texture_flag_ready)) {
        zest__cleanup_texture_vk_handles(texture);
    }

    texture->flags &= ~zest_image_flag_use_filtering;
    zest__process_texture_images(texture, 0);

    zest_AcquireGlobalCombinedSampler2d(font->texture);

    font->pipeline_template = ZestRenderer->pipeline_templates.fonts;
    font->shader_resources = zest_CreateShaderResources(font->name.str);
    zest_ForEachFrameInFlight(fif) {
        zest_AddDescriptorSetToResources(font->shader_resources, ZestRenderer->uniform_buffer->descriptor_set[fif], fif);
        zest_AddDescriptorSetToResources(font->shader_resources, ZestRenderer->global_set, fif);
    }
    zest_ValidateShaderResource(font->shader_resources);
}

void zest__cleanup_font(zest_font_t *font) {
    zest_vec_free(font->characters);
    zest_texture texture = (zest_texture)zest__get_store_resource(&ZestRenderer->textures, font->texture.value);
    if (ZEST_VALID_HANDLE(texture)) {
        zest__cleanup_texture(texture);
    }
    zest_FreeShaderResources(font->shader_resources);
    zest_FreeText(&font->name);
    ZEST__FREE(font);
}
//-- End Fonts

//-- Start Font Drawing API
void zest_SetMSDFFontDrawing(zest_layer_handle layer, zest_font_t *font) {
    ZEST_ASSERT_HANDLE(layer);	//Not a valid handle!
    ZEST_ASSERT_HANDLE(font);	//Not a valid handle!
    zest_EndInstanceInstructions(layer);
    zest_StartInstanceInstructions(layer);
    zest_texture texture = (zest_texture)zest__get_store_resource(&ZestRenderer->textures, font->texture.value);
    ZEST_ASSERT_HANDLE(texture);    //Not a valid texture resource. Make sure the font is properly loaded
    ZEST_ASSERT(ZEST__FLAGGED(texture->flags, zest_texture_flag_ready));        //Make sure the font is properly loaded or wasn't recently deleted
    layer->current_instruction.pipeline_template = font->pipeline_template;
    layer->current_instruction.shader_resources = font->shader_resources;
    layer->current_instruction.draw_mode = zest_draw_mode_text;
    layer->current_instruction.asset = font;
    zest_font_push_constants_t push = { 0 };

    //Font defaults.
    push.shadow_vector.x = 1.f;
    push.shadow_vector.y = 1.f;
    push.shadow_smoothing = 0.2f;
    push.shadow_clipping = 0.f;
    push.radius = 25.f;
    push.bleed = 0.25f;
    push.aa_factor = 5.f;
    push.thickness = 5.5f;
    push.font_texture_index = texture->bindless_index[zest_combined_image_sampler_2d_binding];

    (*(zest_font_push_constants_t *)layer->current_instruction.push_constant) = push;
    layer->last_draw_mode = zest_draw_mode_text;
}

void zest_SetMSDFFontShadow(zest_layer_handle layer, float shadow_length, float shadow_smoothing, float shadow_clipping) {
    zest_vec2 shadow = zest_NormalizeVec2(zest_Vec2Set(1.f, 1.f));
    zest_font_push_constants_t *push = (zest_font_push_constants_t *)layer->current_instruction.push_constant;
    push->shadow_vector.x = shadow.x * shadow_length;
    push->shadow_vector.y = shadow.y * shadow_length;
    push->shadow_smoothing = shadow_smoothing;
    push->shadow_clipping = shadow_clipping;
}

void zest_SetMSDFFontShadowColor(zest_layer_handle layer, float red, float green, float blue, float alpha) {
    ZEST_ASSERT_HANDLE(layer);	//Not a valid handle!
    zest_font_push_constants_t *push = (zest_font_push_constants_t *)layer->current_instruction.push_constant;
    push->shadow_color = zest_Vec4Set(red, green, blue, alpha);
}

void zest_TweakMSDFFont(zest_layer_handle layer, float bleed, float thickness, float aa_factor, float radius) {
    ZEST_ASSERT_HANDLE(layer);	//Not a valid handle!
    zest_font_push_constants_t *push = (zest_font_push_constants_t *)layer->current_instruction.push_constant;
    push->radius = radius;
    push->bleed = bleed;
    push->aa_factor = aa_factor;
    push->thickness = thickness;
}

void zest_SetMSDFFontBleed(zest_layer_handle layer, float bleed) {
    ZEST_ASSERT_HANDLE(layer);	//Not a valid handle!
    zest_font_push_constants_t *push = (zest_font_push_constants_t *)layer->current_instruction.push_constant;
    push->bleed = bleed;
}

void zest_SetMSDFFontThickness(zest_layer_handle layer, float thickness) {
    ZEST_ASSERT_HANDLE(layer);	//Not a valid handle!
    zest_font_push_constants_t *push = (zest_font_push_constants_t *)layer->current_instruction.push_constant;
    push->thickness = thickness;
}

void zest_SetMSDFFontAAFactor(zest_layer_handle layer, float aa_factor) {
    ZEST_ASSERT_HANDLE(layer);	//Not a valid handle!
    zest_font_push_constants_t *push = (zest_font_push_constants_t *)layer->current_instruction.push_constant;
    push->aa_factor = aa_factor;
}

void zest_SetMSDFFontRadius(zest_layer_handle layer, float radius) {
    ZEST_ASSERT_HANDLE(layer);	//Not a valid handle!
    zest_font_push_constants_t *push = (zest_font_push_constants_t *)layer->current_instruction.push_constant;
    push->radius = radius;
}

void zest_SetMSDFFontDetail(zest_layer_handle layer, float bleed) {
    ZEST_ASSERT_HANDLE(layer);	//Not a valid handle!
    zest_font_push_constants_t *push = (zest_font_push_constants_t *)layer->current_instruction.push_constant;
    push->bleed = bleed;
}

float zest_DrawMSDFText(zest_layer_handle layer, const char *text, float x, float y, float handle_x, float handle_y, float size, float letter_spacing) {
    ZEST_ASSERT_HANDLE(layer);	//Not a valid handle!
    ZEST_ASSERT(layer->current_instruction.draw_mode == zest_draw_mode_text);        //Call zest_StartFontDrawing before calling this function

    zest_font_t *font = (zest_font_t*)(layer->current_instruction.asset);

    size_t length = strlen(text);
    if (length <= 0) {
        return 0;
    }

    float scaled_line_height = font->font_size * size;
    float scaled_offset = font->max_yoffset * size;
    x -= zest_TextWidth(font, text, size, letter_spacing) * handle_x;
    y -= (scaled_line_height * handle_y) - scaled_offset;

    float xpos = x;

    zest_texture texture = (zest_texture)zest__get_store_resource(&ZestRenderer->textures, font->texture.value);
    ZEST_ASSERT_HANDLE(texture);    //Not a valid texture resource

    for (int i = 0; i != length; ++i) {
        zest_sprite_instance_t *font_instance = (zest_sprite_instance_t *)layer->memory_refs[layer->fif].instance_ptr;
        zest_font_character_t *character = &font->characters[text[i]];

        if (character->flags > 0) {
            xpos += character->xadvance * size + letter_spacing;
            continue;
        }

        float width = character->width * size;
        float height = character->height * size;
        float xoffset = character->xoffset * size;
        float yoffset = character->yoffset * size;

        float uv_width = texture->texture.width * (character->uv.z - character->uv.x);
        float uv_height = texture->texture.height * (character->uv.y - character->uv.w);
        float scale = width / uv_width;

        font_instance->size_handle = zest_Pack16bit4SScaledZWPacked(width, height, 0, 4096.f);
        font_instance->position_rotation = zest_Vec4Set(xpos + xoffset, y + yoffset, 0.f, 0.f);
        font_instance->uv = character->uv;
        font_instance->alignment = 1;
        font_instance->color = layer->current_color;
        font_instance->intensity_texture_array = 0;
        font_instance->intensity_texture_array = (zest_uint)(layer->intensity * 0.125f * 4194303.f);

        zest_NextInstance(layer);

        xpos += character->xadvance * size + letter_spacing;
    }

    return xpos;
}

float zest_DrawMSDFParagraph(zest_layer_handle layer, const char *text, float x, float y, float handle_x, float handle_y, float size, float letter_spacing, float line_height) {
    ZEST_ASSERT_HANDLE(layer);	//Not a valid handle!
    ZEST_ASSERT(layer->current_instruction.draw_mode == zest_draw_mode_text);        //Call zest_StartFontDrawing before calling this function

    zest_font_t *font = (zest_font_t*)(layer->current_instruction.asset);

    zest_texture texture = (zest_texture)zest__get_store_resource(&ZestRenderer->textures, font->texture.value);
    ZEST_ASSERT_HANDLE(texture);    //Not a valid texture resource

    size_t length = strlen(text);
    if (length <= 0) {
        return 0;
    }

    float scaled_line_height = line_height * font->font_size * size;
    float scaled_offset = font->max_yoffset * size;
    float paragraph_height = scaled_line_height * (handle_y);
    for (int i = 0; i != length; ++i) {
        zest_font_character_t *character = &font->characters[text[i]];
        if (character->flags == zest_character_flag_new_line) {
            paragraph_height += scaled_line_height;
        }
    }

    x -= zest_TextWidth(font, text, size, letter_spacing) * handle_x;
    y -= (paragraph_height * handle_y) - scaled_offset;

    float xpos = x;

    for (int i = 0; i != length; ++i) {
        zest_sprite_instance_t *font_instance = (zest_sprite_instance_t *)layer->memory_refs[layer->fif].instance_ptr;
        zest_font_character_t *character = &font->characters[text[i]];

        if (character->flags == zest_character_flag_skip) {
            xpos += character->xadvance * size + letter_spacing;
            continue;
        } else if (character->flags == zest_character_flag_new_line) {
            y += scaled_line_height;
            xpos = x;
            continue;
        }

        float width = character->width * size;
        float height = character->height * size;
        float xoffset = character->xoffset * size;
        float yoffset = character->yoffset * size;

        float uv_width = texture->texture.width * (character->uv.z - character->uv.x);
        float uv_height = texture->texture.height * (character->uv.y - character->uv.w);
        float scale = width / uv_width;

        font_instance->size_handle = zest_Pack16bit4SScaledZWPacked(width, height, 0, 4096.f);
        font_instance->position_rotation = zest_Vec4Set(xpos + xoffset, y + yoffset, 0.f, 0.f);
        font_instance->uv = character->uv;
        font_instance->alignment = 1;
        font_instance->color = layer->current_color;
        font_instance->intensity_texture_array = (zest_uint)(layer->intensity * 0.125f * 4194303.f);

        zest_NextInstance(layer);

        xpos += character->xadvance * size + letter_spacing;
    }

    return xpos;
}

float zest_TextWidth(zest_font_t *font, const char *text, float font_size, float letter_spacing) {
    ZEST_ASSERT_HANDLE(font);	//Not a valid handle!

    float width = 0;
    float max_width = 0;

    size_t length = strlen(text);

    for (int i = 0; i != length; ++i) {
        zest_font_character_t *character = &font->characters[text[i]];

        if (character->flags == zest_character_flag_new_line) {
            width = 0;
        }

        width += character->xadvance * font_size + letter_spacing;
        max_width = ZEST__MAX(width, max_width);
    }

    return max_width;
}


zest_layer_handle zest_CreateFontLayer(const char *name) {
    zest_layer_builder_t builder = zest_NewInstanceLayerBuilder(sizeof(zest_sprite_instance_t));
    zest_layer_handle layer = zest_FinishInstanceLayer(name, &builder);
    return layer;
}