#pragma once
#include "core/basic_types.h"

struct HashedString {
	u32 hash;
	const char* str;
};

template <int N>
constexpr unsigned int djb2StringHash(const char* str) {
	return (djb2StringHash<N - 1>(str) * 33) ^ str[N - 1];
}

template <>
constexpr unsigned int djb2StringHash<0>(const char* str) {
	return 5381;
}

template<int N>
constexpr unsigned int compileTimeStringHash(const char(&str)[N]) {
	return djb2StringHash<N>(str);
}

#define HS(s) (HashedString{compileTimeStringHash(s), s})





// ------------------------------------------------------------------//
// An alternate method
#if 0

template<size_t N>
static constexpr inline unsigned int hashloop(const char(&str)[N]) {
	unsigned int hash = 0;
	for (size_t i = 0; i < N; ++i)
		hash = 65599 * hash + str[i];
	return hash ^ (hash >> 16);
}

// we force the compiler to evaluate the constexpr at compile time by using it as a template argument to this function
template<unsigned int N>
struct CompileTimeStringHashCompilerTricker {
	static const unsigned int THE_HASH = N;
};

#define HS(s) (HashedString{CompileTimeStringHashCompilerTricker<hashloop(s)>::THE_HASH, s})

#endif