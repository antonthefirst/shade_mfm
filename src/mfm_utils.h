#pragma once
#include "core/vec2.h"

inline ivec2 getSiteCoord(int i) {
	switch (i) {
	case 0:  return ivec2(0, 0);
	case 1:  return ivec2(-1, 0);
	case 2:  return ivec2(0, -1);
	case 3:  return ivec2(0, +1);
	case 4:  return ivec2(+1, 0);
	case 5:  return ivec2(-1, -1);
	case 6:  return ivec2(-1, +1);
	case 7:  return ivec2(+1, -1);
	case 8:  return ivec2(+1, +1);
	case 9:  return ivec2(-2, 0);
	case 10: return ivec2(0, -2);
	case 11: return ivec2(0, +2);
	case 12: return ivec2(+2, 0);
	case 13: return ivec2(-2, -1);
	case 14: return ivec2(-2, +1);
	case 15: return ivec2(-1, -2);
	case 16: return ivec2(-1, +2);
	case 17: return ivec2(+1, -2);
	case 18: return ivec2(+1, +2);
	case 19: return ivec2(+2, -1);
	case 20: return ivec2(+2, +1);
	case 21: return ivec2(-3, 0);
	case 22: return ivec2(0, -3);
	case 23: return ivec2(0, 3);
	case 24: return ivec2(3, 0);
	case 25: return ivec2(-2, -2);
	case 26: return ivec2(-2, +2);
	case 27: return ivec2(+2, -2);
	case 28: return ivec2(+2, +2);
	case 29: return ivec2(-3, -1);
	case 30: return ivec2(-3, +1);
	case 31: return ivec2(-1, -3);
	case 32: return ivec2(-1, +3);
	case 33: return ivec2(+1, -3);
	case 34: return ivec2(+1, +3);
	case 35: return ivec2(+3, -1);
	case 36: return ivec2(+3, +1);
	case 37: return ivec2(-4, 0);
	case 38: return ivec2(0, -4);
	case 39: return ivec2(0, +4);
	case 40: return ivec2(+4, 0);
	default: return ivec2(0, 0);
	};
}
