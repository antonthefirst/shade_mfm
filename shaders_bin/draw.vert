#version 430 core

#line 1 "shaders/draw_shared.inl"
#ifdef _WIN32
typedef unsigned int uint;
#endif

#ifdef _WIN32
struct DrawUPC {
#else
layout(push_constant) uniform UPC {
#endif
	vec2 camera_from_world_shift; vec2 camera_from_world_scale;
	float inv_camera_aspect; float screen_from_grid_scale; float event_window_vis;
};

#line 1 "shaders/draw.vert"
//include "draw_shared.inl"

layout(location = 0) in vec2 pos_in;
layout(location = 1) in vec2 st_in;

out gl_PerVertex { vec4 gl_Position; };
layout(location = 0) out vec2 st;

void main()
{
	vec2 pos = camera_from_world_shift + pos_in * camera_from_world_scale;
	pos.x *= inv_camera_aspect;
	gl_Position = vec4(pos.x, pos.y, 0.f, 1.0);
	gl_Position.y = -gl_Position.y;
	st = st_in;
}
