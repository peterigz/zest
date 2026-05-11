#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : require

const float packed_scale_max = 128.0 / 32767.0;

layout(binding = 7) uniform UboView
{
    mat4 view;
    mat4 proj;
    vec2 screen_size;
	uint millisecs;
    float timer_lerp;
    float update_time;
} ub[];

struct ImageData {
	vec4 uv;
    vec2 padding;
	vec2 image_size;
	uint texture_array_index;
	float animation_frames;
    float max_radius;
};

struct ribbon_segment {
    vec3 position;			            // xyz = position, w = width                
	uint intensity_gradient_map;		//Multiplier for the color of the ribbon
	uint curved_alpha;					//Sharpness and dissolve amount value for fading the image 
	uint padding[3];
};

struct tfx_ribbon {
    vec4 position;                      //normalised age is in w
    float width;
	uint segment_start_index;
	uint flags;
	uint padding_pre_quat;
	uvec2 quaternion;                   //16-bit snorm quaternion: .x = X|Y, .y = Z|W
    uint emitter_index;
	uint texture_indexes;
	uint intensity_gradient_map;		//Multiplier for the color of the ribbon
	uint curved_alpha;	    			//Sharpness and dissolve amount value for fading the image
	uint _padding[2];                   //Padding to 64 bytes for std430 alignment
};

struct tfx_emitter {
	uvec2 quaternion;
	uint lookup_offset;
	uint angle_type;
	vec3 position;
	float heat_response_boost;
	vec3 captured_position;
	float heat_response_sharpness;
	vec3 scale;
	float heat_response_curve;
	vec3 fixed_angle_normal;
	int padding;
};

layout(binding = 5) readonly buffer InImageData {
	ImageData data[];
} images[];

layout(binding = 5) readonly buffer InRibbonSegments {
	ribbon_segment data[];
} segments[];

layout(binding = 5) readonly buffer InRibbonInstances {
	tfx_ribbon data[];
} ribbons[];

layout(binding = 5) readonly buffer InEmitters {
	tfx_emitter data[];
} emitters[];

layout(push_constant) uniform push_constants
{
    vec4 camera_position;           
    uint segment_count;             
    uint tessellation;              
    uint index_offset;              
    uint vertex_offset;              
    uint ribbon_count;              
    uint ribbon_offset;
    uint segment_offset;
	uint uniform_index;
	uint emitters_index;
	uint graphs_index;
	uint ribbons_index;
	uint ribbon_segments_index;
	uint vertexes_index;
	uint indexes_index;
	uint image_data_index;
	uint sampler_index;
	uint particle_texture_index;
	uint color_ramp_texture_index;
    float lerp;
} pc;

layout(location = 0) in vec3 vertex_position;
layout(location = 1) in uint segment_index;
layout(location = 2) in vec2 uv_offset_scale;
layout(location = 3) in uint ribbon_index;
layout(location = 4) in uint clipped;

layout(location = 0) out vec3 out_tex_coord;
layout(location = 1) out ivec3 out_texture_indexes;
layout(location = 2) out vec4 out_intensity_curved_alpha_map;
layout(location = 3) out flat vec3 out_heat_response;

vec2 unpack16bit_sscaled(uint packed) {
    int x_scaled = int(packed & 0xFFFFu);
    int y_scaled = int((packed >> 16u) & 0xFFFFu);
    vec2 unpacked;
    return vec2(float(x_scaled) * packed_scale_max, float(y_scaled) * packed_scale_max);
}

void main() {
	gl_Position = (ub[pc.uniform_index].proj * ub[pc.uniform_index].view * vec4(vertex_position, 1.0));
	uint current_segment_index = (segment_index & 0x000007FF);
	float ribbon_position = float((segment_index & 0x007FF000) >> 12) / 2047.0;
	ribbon_segment segment = segments[pc.ribbon_segments_index].data[current_segment_index];
	tfx_ribbon ribbon = ribbons[pc.ribbons_index].data[ribbon_index];
	tfx_emitter emitter = emitters[pc.emitters_index].data[ribbon.emitter_index];
	uint image_index = ribbon.texture_indexes & 0x00001FFF;

	//Calculate the uv coords across the width of the ribbon
	float tessellation = float((segment_index & 0xE0000000) >> 29);
	float tessellation_index = float((segment_index & 0x1C000000) >> 26);
	float side = float((segment_index & 0x02000000) >> 25);
	float t = tessellation_index / tessellation;  
	vec4 uv = images[pc.image_data_index].data[image_index].uv;
	float texture_uv_width = uv.z - uv.x;

	// In order to wrap the texture using images that are stored on in a texture array of sprite sheets we have to
	// ping pong the texture coordinates. This is the only solution I found to avoid the points along the ribbon
	// where the texture wraps and you end up with a small 1 ribbon segment sized texture that is flipped due to 
	// the wrapping of coordinates. Other alternatives mean you either have to add extra vertices when building the
	// ribbon or just using a separate texture array, neither of which I liked. Maybe there is another solution I'm 
	// overlooking though of course!
	float scaled_position = ribbon_position * uv_offset_scale.y + uv_offset_scale.x;
	float wrap_index = floor(scaled_position);		// Which wrap iteration we're on
	float wrap_fraction = fract(scaled_position);

	// If wrap_index is odd, reverse the direction (ping-pong)
	if(mod(wrap_index, 2.0) == 1.0) {
		wrap_fraction = 1.0 - wrap_fraction;  // Reverse direction for odd wraps
	}

	/*
	// Using multiply and abs to avoid branching(?) for future profiling experiments
	float ping_pong = abs(mod(wrap_index, 2.0) - wrap_fraction);
	// or alternatively:
	// float ping_pong = abs(fract(scaled_position * 0.5) * 2.0 - 1.0);
	*/

	float uv_x = wrap_fraction * texture_uv_width + uv.x;
	float uv_y = (t * 0.5 + (side * 0.5)) * (uv.w - uv.y) + uv.y;
	vec2 ribbon_intensity_gradient_map = unpack16bit_sscaled(ribbon.intensity_gradient_map);
	vec2 ribbon_curved_alpha = unpack16bit_sscaled(ribbon.curved_alpha);
	vec2 intensity_gradient_map = unpack16bit_sscaled(segment.intensity_gradient_map);
	vec2 curved_alpha = unpackUnorm2x16(segment.curved_alpha);

	int life = int(ribbon_position * 255);
	out_tex_coord = vec3(vec2(uv_x, uv_y), images[pc.image_data_index].data[image_index].texture_array_index);
	out_texture_indexes = ivec3((ribbon.texture_indexes & 0xFF000000) >> 24, (ribbon.texture_indexes & 0x00FF0000) >> 16, life);
	out_intensity_curved_alpha_map = vec4(intensity_gradient_map.x * ribbon_intensity_gradient_map.x * clipped, curved_alpha.x * ribbon_curved_alpha.x, curved_alpha.y * ribbon_curved_alpha.y, intensity_gradient_map.y * ribbon_intensity_gradient_map.y);
	out_heat_response = vec3(emitter.heat_response_boost, emitter.heat_response_sharpness, emitter.heat_response_curve);
}