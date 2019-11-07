layout (binding = 0, rgba32ui) uniform uimage2D img_prng_state;
layout (binding = 1, rgba32ui) uniform uimage2D img_site_bits;
layout (binding = 2, r32ui)    uniform uimage2D img_vote;
layout (binding = 3, rgba8ui)  uniform uimage2D img_color;
layout (binding = 4, r32ui)    uniform uimage2D img_event_count;
layout (binding = 5, rgba32ui) uniform uimage2D img_dev;

/*
layout(std430, binding = 0) coherent 
buffer Stats 
{
	WorldStats stats;
};
layout(std430, binding = 1)
buffer Site 
{
	SiteInfo site_info;
};
layout(std430, binding = 2)
buffer Control 
{
	ControlState ctrl;
};
*/

layout (local_size_x = GROUP_SIZE_X, local_size_y = GROUP_SIZE_Y, local_size_z = 1) in;