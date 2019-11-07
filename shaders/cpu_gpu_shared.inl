#if defined(_WIN32) || defined(__GNUC__)
#pragma once
typedef unsigned int uint;
struct uvec4 {
	unsigned int x,y,z,w;
};
#else
#endif

#define GROUP_SIZE_X 8
#define GROUP_SIZE_Y 8

#define STAGE_RESET         0
#define STAGE_CLEAR_STATS   1
#define STAGE_VOTE          2
#define STAGE_EVENT         3
#define STAGE_COMPUTE_STATS 4
#define STAGE_SITE_INFO     5
#define STAGE_RENDER        6

#define BITS_PER_COMPONENT 32

#define ATOM_BITS                 96
#define ATOM_ECC_GLOBAL_OFFSET    87
#define ATOM_ECC_BITSIZE          9
#define ATOM_TYPE_GLOBAL_OFFSET   71
#define ATOM_TYPE_BITSIZE         16
#define ATOM_TYPE_COMPONENT    (ATOM_TYPE_GLOBAL_OFFSET/BITS_PER_COMPONENT)
#define ATOM_TYPE_LOCAL_OFFSET (ATOM_TYPE_GLOBAL_OFFSET%BITS_PER_COMPONENT)
#define ATOM_TYPE_BITMASK       0xffff
#define SYMMETRY_GLOBAL_OFFSET  86 // upper bit of type
#define SYMMETRY_COMPONENT      (SYMMETRY_GLOBAL_OFFSET/BITS_PER_COMPONENT)
#define SYMMETRY_LOCAL_OFFSET   (SYMMETRY_GLOBAL_OFFSET%BITS_PER_COMPONENT)
#define SYMMETRY_BITSIZE        1
#define SYMMETRY_BITMASK        0x1
#define SYMMETRY_SIFT           (SYMMETRY_BITMASK << SYMMETRY_LOCAL_OFFSET)

#define TYPE_COUNTS 32

struct WorldStats {
	uint counts[TYPE_COUNTS];
	
	uint event_count_this_batch;
	uint event_count_min;
	uint event_count_max;
	uint pad0;

};

struct SiteInfo {
	 uvec4 event_layer;
	 uvec4 base_layer;
	 uvec4 dev;
	 int event_ocurred_signal; int site_pad0, site_pad1, site_pad2;
};

struct ControlState {
	int supress_events; int event_ocurred; int ctrl_pad1, ctrl_pad2;
};

#ifdef _WIN32
typedef unsigned int uint;
#endif

#ifdef _WIN32
struct ComputeUPC {
#else
layout(push_constant) uniform UPC {
#endif
	uint dispatch_counter; uint stage; ivec2 site_info_idx;
	bool break_on_event;
};