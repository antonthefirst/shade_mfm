#pragma once
#include "basic_types.h"
#include <math.h>

template <typename T>
inline const T& min(const T& a, const T& b) {
	return b < a ? b : a;
}
template <typename T>
inline const T& max(const T& a, const T& b) {
	return  a < b ? b : a;
}
template <typename T>
inline void swap(T& a, T& b) {
	T temp = a;
	a = b;
	b = temp;
}
template <typename T>
inline T lerp(const T& a, const T& b, float t) {
	return a + (b-a)*t;
}
template <typename T>
inline T clamp(const T& val, const T& min_val, const T& max_val) {
	return max(min_val, min(max_val, val));
}

inline float sqr(float a) { return a*a; }

#define EXP 2.71828182845904523536f
#define LOG2E 1.44269504088896340736f
#define LOG10E 0.434294481903251827651f
#define LN2 0.693147180559945309417f
#define LN10 2.30258509299404568402f
#define PI 3.14159265358979323846f
#define PI_2 1.57079632679489661923f
#define PI_4 0.785398163397448309616f
//#define 1_PI 0.318309886183790671538f
//#define 2_PI 0.636619772367581343076f
//#define 1_SQRTPI 0.564189583547756286948f
//#define 2_SQRTPI 1.12837916709551257390f
#define SQRT2 1.41421356237309504880f
#define SQRT_2 0.707106781186547524401f
#define TAU (PI*2.f)
#define GOLDEN_ANGLE 2.39996322972865332f

// rotate vector (dx,dy) by rotation (rx,ry)
void rotate(float rx, float ry, float& dx, float &dy);
void rotation(float rad, float& dx, float& dy);
void normalize(float& dx, float& dy);

inline float smoothstep(float f) { return 3.f*f*f - 2.f*f*f*f; }
inline float fract(float f) { return f - floorf(f);  }
inline float lerp(float a, float b, float t) { return a + (b-a)*t; }

inline u32 pack_unorm(float x, float y, float z, float w) {
#define TOBYTE(x) (int(max(0.f, min(255.f, x*255.f + 0.5f))))
	return (TOBYTE(w) << 24) | (TOBYTE(z) << 16) | (TOBYTE(y) << 8) | (TOBYTE(x));
}
template<typename T>
inline u32 pack_unorm(T v) {
#define TOBYTE(x) (int(max(0.f, min(255.f, x*255.f + 0.5f))))
	return (TOBYTE(v.w) << 24) | (TOBYTE(v.z) << 16) | (TOBYTE(v.y) << 8) | (TOBYTE(v.x));
}

#include "basic_types.h"
inline u32 WangHash(u32 seed)
{
    seed = (seed ^ 61) ^ (seed >> 16);
    seed *= 9;
    seed = seed ^ (seed >> 4);
    seed *= 0x27d4eb2d;
    seed = seed ^ (seed >> 15);
    return seed;
}
inline float WangHashFloat(u32 seed)	{ return float(WangHash(seed) % 16777216)*(1.0f / 16777215.0f); }
inline float WangHashFloat(u32 seed, float val_min, float val_max) { return val_min + (val_max-val_min)*WangHashFloat(seed); }