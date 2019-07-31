#pragma once
#include "core/basic_types.h" // req for shared
#include "core/vec2.h"        // req for shared
#include "shaders/cpu_gpu_shared.inl"
#include "shaders/defines.inl"
#include "core/string_range.h"
#include "core/container.h"

#define NUM_INTERNAL_DATA_MEMBERS (2)
#define NO_OFFSET (-1)

struct DataField {
	StringRange name;
	int global_offset;
	int bitsize;
	int type;
	bool internal;
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

void dataAddInternalAndPickOffsets(Bunch<DataField>& data);