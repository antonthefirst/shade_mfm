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
