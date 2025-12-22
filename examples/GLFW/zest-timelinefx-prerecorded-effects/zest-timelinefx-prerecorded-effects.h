#pragma once

#include <GLFW/glfw3.h>
#include <zest.h>
#include "implementations/impl_imgui.h"
#include "imgui/imgui.h"
#include <imgui/misc/freetype/imgui_freetype.h>
#include "impl_timelinefx.h"
#include <imgui/backends/imgui_impl_glfw.h>

struct AnimationComputeConstants {
	tfxU32 total_sprites;
	tfxU32 total_animation_instances;
	tfxU32 flags;
};

struct ComputePushConstants {
	zest_uint instances_size;
	zest_uint has_animated_shapes;
	zest_uint total_sprites_to_draw;
	zest_uint sprite_data_offset;
};

struct tfxPrerecordedExample {
	zest_context context;
	zest_device device;
	zest_compute_handle compute;
	zest_compute_handle bounding_box_compute;
	bool left_mouse_clicked;
	bool right_mouse_clicked;

	//The sprite data buffer contains all of the baked in sprites for effects that you can upload to the GPU
	zest_buffer sprite_data_buffer;
	//The image data buffer contains data about the textures used to draw the sprites such as uv coordinates. The sprite
	//data contains an index to look up this data
	zest_buffer image_data_buffer;
	//We also need to store some additional emitter property data such as the sprite handle which is looked up by the 
	//compute shader each frame
	zest_buffer emitter_properties_buffer;
	//This example we are building pre-recording the effects from an effects library as apposed to just loading in a
	//sprite data file that has all the effects and sprite data pre-built. Therefore if we want bounding boxed for the 
	//effects we will need to calculate those after recording the effects (this is optional). With the bounding boxes
	//we can cull effects that are outside the viewing frustum.
	zest_buffer bounding_boxes;

	//For GPU drivers that don't have the capability to write directly to the GPU buffer you will need
	//staging buffers to write to first before uploading those to the GPU device buffers. We only need to upload
	//the instances of each animation and the offsets array which are very small so not much overhead at all.
	zest_buffer animation_instances_staging_buffer[ZEST_MAX_FIF];
	zest_buffer offsets_staging_buffer[ZEST_MAX_FIF];
	//The compute shader that interpolates all the sprite data
	zest_shader_handle playback_shader;

	tfx_render_resources_t tfx_rendering;

	zest_imgui_t imgui;
	tfx_gpu_shapes gpu_image_data;
	zest_timer_t timer;
	zest_camera_t camera;

	zest_layer_handle mesh_layer;			//To draw the floor plain
	zest_pipeline_template mesh_pipeline;
	zest_atlas_region_t floor_image;
	zest_image_handle floor_texture;
	zest_sampler_handle sampler;
	zest_uint sampler_index;
	tfx_random_t random;

	double mouse_x, mouse_y;
	double mouse_delta_x, mouse_delta_y;

	//Indexes for the compute shader pipeline_templates
	zest_compute_handle compute_pipeline_3d;
	zest_compute_handle bb_compute_pipeline_3d;

	tfx_animation_manager animation_manager_3d;
	AnimationComputeConstants animation_manager_push_constants;
	tfx_sprite_data_settings_t anim_test;
	zest_millisecs record_time;
	bool effect_is_3d;
	zest_vec4 planes[6];

	zest_microsecs trigger_effect;
};

void SpriteComputeFunction(zest_command_list command_list, void *user_data);
void RecordComputeSprites(zest_command_list command_list, void *user_data);
void InitExample(tfxPrerecordedExample *example);
void PrepareComputeForEffectPlayback(tfxPrerecordedExample *example);
void UploadBuffers(tfxPrerecordedExample *example);
void UpdateUniform3d(tfxPrerecordedExample *game);
void Update(zest_microsecs elapsed, void *data);
void BuildUI(tfxPrerecordedExample *example);
bool CullAnimationInstancesCallback(tfx_animation_manager animation_manager, tfx_animation_instance_t *instance, tfx_frame_meta_t *frame_meta, void *user_data);
tfx_vec3_t ScreenRay(zest_context context, float x, float y, float depth_offset, zest_vec3 &camera_position);

