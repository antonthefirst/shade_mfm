#pragma once
#include "core/basic_types.h"

struct Input;

struct HashedString {
	u32 hash;
	const char* str;
};


void gtimer_reset();
void gtimer_start(const char* name);
void gtimer_stop();

void timerUI(Input& in);


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

#else
template <int N>
static  unsigned int djb2HashStr(const char* str) {
	return (djb2HashStr<N - 1>(str) * 33) ^ str[N - 1];
}
template <>
constexpr unsigned int djb2HashStr<0>(const char* str) {
	return 5381;
}

template<int N>
constexpr unsigned int compileTimeHash(const char(&str)[N]) {
	return djb2HashStr<N>(str);
}
#define HS(s) (HashedString{compileTimeHash(s), s})
#endif

void chashtimer_start(HashedString hs);
void chashtimer_stop();


void ctimer_reset();
#define ctimer_start(s) chashtimer_start(HS(s))
#define ctimer_stop() chashtimer_stop()
