#pragma once

#include <zest.h>
#include "implementations/impl_imgui.h"
#include "implementations/impl_glfw.h"
#include "implementations/impl_imgui_glfw.h"
#include "imgui/imgui.h"
#include <imgui/misc/freetype/imgui_freetype.h>
#include <imgui/backends/imgui_impl_glfw.h>
#include "../../TimelineFXLib/timelinefx.h"

struct AnimationComputeConstants {
	tfxU32 total_sprites;
	tfxU32 total_animation_instances;
	tfxU32 flags;
};

struct tfx_render_resources_t {
	zest_texture particle_texture;
	zest_texture color_ramps_texture;
	zest_draw_routine draw_routine;
	zest_layer layer;
	zest_descriptor_buffer uniform_buffer_3d;
	zest_descriptor_set uniform_buffer_descriptor_set;
	zest_descriptor_buffer image_data;
	zest_descriptor_set particle_descriptor;
	zest_pipeline pipeline;
	zest_shader_resources shader_resource;
	zest_set_layout descriptor_layout;
	zest_descriptor_set_t descriptor_set;
	zest_shader fragment_shader;
	zest_shader vertex_shader;
};

struct ComputeExample {
	zest_compute compute;
	zest_compute bounding_box_compute;
	bool left_mouse_clicked;
	bool right_mouse_clicked;

	//The sprite data buffer contains all of the baked in sprites for effects that you can upload to the GPU
	zest_descriptor_buffer sprite_data_buffer;
	//The sprite buffer is filled each frame by the compute shader and passed to the vertex shader to draw
	//all of the billboard instances
	zest_descriptor_buffer sprite_buffer;
	//The animation instances buffer contains each animation instance that you want to draw each frame. So 
	//for example maybe you have an explosion effect that you want to draw several times in different locations,
	//each animation instance will contain position and other data about the animation and the compute shader
	//will use that to prepare the sprite buffer with the next frame of animation to be drawn
	zest_descriptor_buffer animation_instances_buffer;
	//becuase the compute shader just draws all animations in one go, for each instance it draws it needs to know the
	//offsets inside the sprite data from where to get the data from to build the sprite buffer each frame.
	zest_descriptor_buffer offsets_buffer;
	//The image data buffer contains data about the textures used to draw the sprites such as uv coordinates. The sprite
	//data contains an index to look up this data
	zest_descriptor_buffer image_data_buffer;
	//We also need to store some additional emitter property data such as the sprite handle which is looked up by the 
	//compute shader each frame
	zest_descriptor_buffer emitter_properties_buffer;
	//This example we are building pre-recording the effects from an effects library as apposed to just loading in a
	//sprite data file that has all the effects and sprite data pre-built. Therefore if we want bounding boxed for the 
	//effects we will need to calculate those after recording the effects (this is optional). With the bounding boxes
	//we can cull effects that are outside the viewing frustum.
	zest_descriptor_buffer bounding_boxes;

	//For GPU drivers that don't have the capability to write directly to the GPU buffer you will need
	//staging buffers to write to first before uploading those to the GPU device buffers. We only need to upload
	//the instances of each animation and the offsets array which are very small so not much overhead at all.
	zest_buffer animation_instances_staging_buffer[ZEST_MAX_FIF];
	zest_buffer offsets_staging_buffer[ZEST_MAX_FIF];
	//The compute shader that calculates the bounding boxes
	zest_shader bb_shader;
	//The compute shader that interpolates all the sprite data
	zest_shader playback_shader;

	tfx_render_resources_t tfx_rendering;

	zest_imgui_t imgui_layer_info;
	tfx_gpu_shapes gpu_image_data;
	zest_push_constants_t push_contants;
	zest_timer timer;
	zest_camera_t camera;

	zest_layer mesh_layer;			//To draw the floor plain
	zest_pipeline mesh_pipeline;
	zest_image floor_image;
	zest_texture floor_texture;
	zest_shader_resources floor_resources;

	//Indexes for the compute shader pipelines
	zest_index compute_pipeline_3d;
	zest_index bb_compute_pipeline_3d;

	tfx_library library;
	tfx_particle_manager pm;
	tfx_animation_manager animation_manager_3d;
	AnimationComputeConstants animation_manager_push_constants;
	tfx_sprite_data_settings_t anim_test;
	zest_millisecs record_time;
	bool effect_is_3d;
	zest_vec4 planes[6];

	zest_microsecs trigger_effect;
};

int SpriteComputeCondition(zest_compute compute);
void SpriteComputeFunction(zest_compute compute);
void BoundingBoxComputeFunction(zest_compute compute, VkCommandBuffer command_buffer);
void RecordComputeSprites(struct zest_work_queue_t *queue, void *data);
void UpdateSpriteResolution(zest_draw_routine routine);
void InitExample(ComputeExample *example);
void PrepareComputeForEffectPlayback(ComputeExample *example);
void PrepareComputeForBoundingBoxCalculation(ComputeExample *example);
void CalculateBoundingBoxes(ComputeExample *example, tfx_animation_manager animation_manager, tfx_effect_emitter_t *effect);
void UploadBuffers(ComputeExample *example);
void UpdateUniform3d(ComputeExample *game);
void Update(zest_microsecs elapsed, void *data);
void BuildUI(ComputeExample *example);
bool CullAnimationInstancesCallback(tfx_animation_manager animation_manager, tfx_animation_instance_t *instance, tfx_frame_meta_t *frame_meta, void *user_data);
tfx_vec3_t ScreenRay(float x, float y, float depth_offset, zest_vec3 &camera_position);

