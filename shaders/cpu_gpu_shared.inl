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


struct WorldStats {
	uint counts[16];
	
	uint event_count_this_batch;
	uint event_count_min;
	uint event_count_max;
	uint pad0;

};

struct SiteInfo {
	 uvec4 event_layer;
	 uvec4 base_layer;
	 uvec4 dev;
};

struct EventJob {
	ivec2 site_idx;
	ivec2 pad0;
};

struct IndirectCommand {
	uint  count;
	uint  primCount;
	uint  firstIndex;
	uint  baseVertex;
	uint  baseInstance;
};