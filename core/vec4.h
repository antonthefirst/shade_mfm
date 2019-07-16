#pragma once
#include "vec3.h"

struct vec4 {
	float x, y, z, w;
	vec4() {}
	explicit vec4(float s);
	vec4(float x, float y, float z, float w);
	vec4(const vec2& v, float z, float w);
	vec4(const vec3& v, float w);
	vec4(const vec2& xy, const vec2& zw);
	vec4 operator+(const vec4& rhs);
	vec4 operator-(const vec4& rhs);
	vec4 operator*(const float s);
	vec4 operator/(const float s);
	bool operator==(const vec4& rhs);
	inline void operator-=(const vec4& rhs) { *this = *this - rhs; }
	inline void operator+=(const vec4& rhs) { *this = *this + rhs; }
	inline void operator/=(const float s) { *this = *this / s; }
	inline void operator*=(const float s) { *this = *this * s; }
	vec2 xy() const;
	vec3 xyz() const;
	vec2 zw() const;
	void xy(vec2 new_xy);
	void zw(vec2 new_zw);
};
vec4 operator+(const vec4& lhs, const vec4& rhs);
vec4 operator-(const vec4& lhs, const vec4& rhs);
vec4 lerp(const vec4& a, const vec4& b, float t);
vec4 maxvec(vec4 a, vec4 b);
vec4 minvec(vec4 a, vec4 b);
vec4 clampvec(vec4 v, vec4 c_min, vec4 c_max);