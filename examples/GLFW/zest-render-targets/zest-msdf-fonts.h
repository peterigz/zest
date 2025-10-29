#include "zest.h"

typedef struct zest_font_t {
    int magic;
    zest_text_t name;
    zest_texture_handle texture;
    zest_pipeline_template pipeline_template;
    zest_shader_resources_handle shader_resources;
    float pixel_range;
    float miter_limit;
    float padding;
    float font_size;
    float max_yoffset;
    zest_uint character_count;
    zest_uint character_offset;
    zest_font_character_t *characters;
} zest_font_t;

typedef struct zest_font_push_constants_t {             //128 bytes seems to be the limit for push constants on AMD cards, NVidia 256 bytes
    zest_vec4 shadow_color;
    zest_vec2 shadow_vector;
    float shadow_smoothing;
    float shadow_clipping;
    float radius;
    float bleed;
    float aa_factor;
    float thickness;
    zest_uint font_texture_index;
} zest_font_push_constants_t;

typedef struct zest_font_character_t {
    char character[4];
    float width;
    float height;
    float xoffset;
    float yoffset;
    float xadvance;
    zest_uint flags;
    float reserved1;
    zest_vec4 uv;
    zest_u64 uv_packed;
    float reserved2[2];
} zest_font_character_t;

// --Font_layer_internal_functions
ZEST_PRIVATE void zest__setup_font_texture(zest_font_t *font);
ZEST_PRIVATE void zest__cleanup_font(zest_font_t *font);

//-----------------------------------------------
//-- Fonts
//-----------------------------------------------
//Load a .zft file for use with drawing MSDF fonts
ZEST_API void zest_LoadMSDFFont(const char *filename, zest_font_t *font);
ZEST_API void zest_DrawFonts(VkCommandBuffer command_buffer, const zest_frame_graph_context_t *context, void *user_data);
//Unload a zest_font and free all it's resources
ZEST_API void zest_FreeFont(zest_font_t *font);
//-- End Fonts


//Create a layer specifically for drawing text using msdf font rendering. See the section Draw_MSDF_font_layers for commands 
//yous can use to setup and draw text. Also see the fonts example.
ZEST_API zest_layer_handle zest_CreateFontLayer(const char *name);

//-----------------------------------------------
//        Draw_MSDF_font_layers
//        Draw fonts at any scale using multi channel signed distance fields. This uses a zest font format
//        ".zft" which contains a character map with uv coords and a png containing the MSDF image for
//        sampling. You can use a simple application for generating these files in the examples:
//        zest-msdf-font-maker
//-----------------------------------------------
//Before drawing any fonts you must call this function in order to set the font you want to use. the zest_font handle can be used to get the vk_descriptor_set and pipeline. See
//zest-fonts for an example.
ZEST_API void zest_SetMSDFFontDrawing(zest_layer_handle font_layer, zest_font_t *font);
//Set the shadow parameters of your font drawing. This only applies to the current call of zest_SetMSDFFontDrawing. For each text that you want different settings for you must
//call a separate zest_SetMSDFFontDrawing to start a new font drawing instruction.
ZEST_API void zest_SetMSDFFontShadow(zest_layer_handle font_layer, float shadow_length, float shadow_smoothing, float shadow_clipping);
ZEST_API void zest_SetMSDFFontShadowColor(zest_layer_handle font_layer, float red, float green, float blue, float alpha);
//Alter the parameters in the shader to tweak the font details and how it's drawn. This can be useful especially for drawing small scaled fonts.
//bleed: Simulates ink bleeding so higher numbers fatten the font but in a more subtle way then altering the thickness.
//thickness: Makes the font bolder with higher numbers. around 5.f
//aa_factor: The amount to alias the font. Lower numbers work better for smaller fonts
//radius: Scale the distance measured to the font edge. Higher numbers make the font thinner
ZEST_API void zest_TweakMSDFFont(zest_layer_handle font_layer, float bleed, float thickness, float aa_factor, float radius);
//The following functions just let you tweak an individual parameter of the font drawing
ZEST_API void zest_SetMSDFFontBleed(zest_layer_handle font_layer, float bleed);
ZEST_API void zest_SetMSDFFontThickness(zest_layer_handle font_layer, float thickness);
ZEST_API void zest_SetMSDFFontAAFactor(zest_layer_handle font_layer, float aa_factor);
ZEST_API void zest_SetMSDFFontRadius(zest_layer_handle font_layer, float radius);
//Draw a single line of text using an MSDF font. You must call zest_SetMSDFFontDrawing before calling this command to specify which font you want to use to draw with and the zest_layer_handle
//that you pass to the function must match the same layer set with that command.
//text:                    The string that you want to display
//x, y:                    The position on the screen.
//handle_x, handle_y    How the text is justified. 0.5, 0.5 would center the text at the position. 0, 0 would left justify and 1, 0 would right justify.
//size:                    The size of the font in pixels.
//letter_spacing        The amount of spacing imbetween letters
ZEST_API float zest_DrawMSDFText(zest_layer_handle font_layer, const char *text, float x, float y, float handle_x, float handle_y, float size, float letter_spacing);
//Draw a paragraph of text using an MSDF font. You must call zest_SetMSDFFontDrawing before calling this command to specify which font you want to use to draw with and the zest_layer_handle
//that you pass to the function must match the same layer set with that command. Use \n to start new lines in the paragraph.
//text:                    The string that you want to display
//x, y:                    The position on the screen.
//handle_x, handle_y    How the text is justified. 0.5, 0.5 would center the text at the position. 0, 0 would left justify and 1, 0 would right justify.
//size:                    The size of the font in pixels.
//letter_spacing        The amount of spacing imbetween letters
ZEST_API float zest_DrawMSDFParagraph(zest_layer_handle font_layer, const char *text, float x, float y, float handle_x, float handle_y, float size, float letter_spacing, float line_height);
//Get the width of a line of text in pixels given the zest_font, test font size and letter spacing that you pass to the function.
ZEST_API float zest_TextWidth(zest_font_t *font, const char *text, float font_size, float letter_spacing);
//-- End Draw MSDF font layers