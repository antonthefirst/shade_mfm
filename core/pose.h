#pragma once
#include "vec4.h"

typedef vec4 pose;

pose identity();
pose inverse(pose t);
pose normalize(pose t);
pose rotation(float rad);
pose scale(float s);
pose trans(vec2 pos, float rot = 0.f, float scale = 1.f);
float scaleof(pose t);
pose operator*(pose lhs, pose rhs);
vec2 operator*(pose lhs, vec2 rhs);
pose operator~(pose uno);

inline bool operator==(const pose& lhs, const pose& rhs) {
	return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z && lhs.w == rhs.w;
}