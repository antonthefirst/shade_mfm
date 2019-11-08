#version 450 core

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

layout(location = 0) out vec4 out_col;
layout(set=0, binding=0) uniform sampler2D tex_col;
layout(location = 0) in vec2 st;

void main()
{
	out_col = vec4(texture(tex_col, st).xyz, 1.0);
}