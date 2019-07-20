#pragma once
#include "core/basic_types.h" // req for shared
#include "core/vec2.h"        // req for shared
#include "shaders/cpu_gpu_shared.inl"
#include "shaders/defines.inl"
#include "core/string_range.h"

#define NO_OFFSET (-1)

enum {
	BASIC_TYPE_Unsigned,
	BASIC_TYPE_Int,
	BASIC_TYPE_Unary,
	BASIC_TYPE_Bool,
	BASIC_TYPE_C2D,
	BASIC_TYPE_S2D,
	BASIC_TYPE_count,
};

inline const char* BASIC_TYPE_NAME(int e) {
	switch (e) {
	case BASIC_TYPE_Unsigned: return "Unsigned";
	case BASIC_TYPE_Int: return "Int";
	case BASIC_TYPE_Unary: return "Unary";
	case BASIC_TYPE_Bool: return "Bool";
	case BASIC_TYPE_C2D: return "C2D";
	case BASIC_TYPE_S2D: return "S2D";
	default: return "Unknown";
	}
};

struct DataField {
	StringRange name;
	int global_offset;
	int bitsize;
	int type;
	void appendDataFunctions(String& text, StringRange class_name);
	void appendLocalDataFunctionDefines(String& text, StringRange class_name, bool define);
	void appendTo(Atom A, String& text) const;
};

inline u32 maskBits(int bitsize) {
	u32 b = 0;
	for (int i = 0; i < bitsize; ++i)
		b |= (1 << i);
	return b;
}
inline int maxUnsignedVal(int bitsize) {
	return (1 << bitsize) - 1;
}
inline int maxSignedVal(int bitsize) {
	return maxUnsignedVal(bitsize - 1);
}
inline int minSignedVal(int bitsize) {
	return -(1 << (bitsize - 1));
}
inline u32 unpackBits(Atom A, int global_offset, int bitsize) {
	return (((uint*)&A)[global_offset / BITS_PER_COMPONENT] >> (global_offset%BITS_PER_COMPONENT)) & maskBits(bitsize);
}
inline int unpackInt(Atom A, int global_offset, int bitsize) {
	u32 b = unpackBits(A, global_offset, bitsize);
	int s = b & (1 << (bitsize - 1));
	if (s != 0)
		for (int i = bitsize; i < 32; ++i)
			b |= (1 << i);
	return int(b);
}

