#pragma once

#include "vec2.h"

struct vec3 {
	float x, y, z;
	vec3() {}
	explicit vec3(float s);
	vec3(float x, float y, float z);
	vec3(const vec2& v, float z);
	vec3 operator+(const vec3& rhs) const;
	vec3 operator-(const vec3& rhs) const;
	vec3 operator*(const float s) const;
	vec3 operator/(const float s) const;
	inline void operator-=(const vec3& rhs) { *this = *this - rhs; }
	inline void operator+=(const vec3& rhs) { *this = *this + rhs; }
	inline void operator/=(const float s) { *this = *this / s; }
	inline void operator*=(const float s) { *this = *this * s; }
	vec2 xy() const;
};

float dot(vec3 a, vec3 b);
float length(vec3 v);
vec3 normalize(vec3);
vec3 maxvec(vec3 a, vec3 b);
vec3 minvec(vec3 a, vec3 b);
vec3 clampvec(vec3 v, vec3 c_min, vec3 c_max);