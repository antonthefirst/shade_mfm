layout(location = 0, rgba32ui) uniform uimage2D img_site_bits;
layout(location = 1) uniform writeonly image2D img_color;
layout(location = 2, rgba32ui) uniform uimage2D img_dev;
layout(location = 3, r32ui) uniform uimage2D img_vote;
layout(location = 4, r32ui) uniform uimage2D img_event_count;
layout(location = 5, rgba32ui) uniform uimage2D img_prng_state;
layout(location = 6) uniform uint dispatch_counter;
layout(location = 7) uniform ivec2 site_info_idx;
layout(location = 8) uniform int stage;
//layout(location = 9)
layout(location = 10) uniform int ZERO;
layout(location = 11) uniform int ONE;
layout(location = 12) uniform int EVENT_WINDOW_RADIUS_2;



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

layout (local_size_x = GROUP_SIZE_X, local_size_y = GROUP_SIZE_Y, local_size_z = 1) in;