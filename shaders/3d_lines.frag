#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec4 frag_color;
layout(location = 1) in vec4 p1;
layout(location = 2) in vec4 p2;
layout(location = 3) in vec3 end;
layout(location = 4) in float millisecs;

layout(location = 0) out vec4 out_color;

float Line(in vec2 p, in vec2 a, in vec2 b) {
	vec2 ba = b - a;
	vec2 pa = p - a;
	float h = clamp(dot(pa, ba) / dot(ba, ba), 0., 1.);
	return length(pa - h * ba);
}

float Fill(float sdf) {
	//return step(0, -sdf);
	return clamp(0.5 - sdf / fwidth(sdf), 0, 1);		//Anti Aliased
}

void main() {
	vec3 line = vec3(p1.xyz - gl_FragCoord.xyz);
	float animated_brightness = step(5, mod(length(line) + (millisecs * 0.05), 10));

	//Line sdf seems to give perfectly fine results, UnevenCapsule would be more accurate though if widths change drastically over the course of the ribbon.
	float radius = p2.w * .5;
	float line_sdf = Line(gl_FragCoord.xy, p1.xy, p2.xy) - radius; 
	
	float brightness = Fill(line_sdf);

	out_color = frag_color * brightness;
	out_color.rgb *= frag_color.a;
}