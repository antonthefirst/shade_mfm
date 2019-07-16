#include "vec3.h"
#include "core/maths.h"


vec3::vec3(float s) : x(s), y(s), z(s) {}

vec3::vec3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}

vec3::vec3(const vec2 & v, float _z) : x(v.x), y(v.y), z(_z) {}

vec3 vec3::operator+(const vec3 & rhs) const {
	return vec3(x + rhs.x, y + rhs.y, z + rhs.z);
}

vec3 vec3::operator-(const vec3 & rhs) const {
	return vec3(x - rhs.x, y - rhs.y, z - rhs.z);
}

vec3 vec3::operator*(const float s) const {
	return vec3(x*s, y*s, z*s);
}

vec3 vec3::operator/(const float s) const {
	return vec3(x / s, y / s, z / s);
}

vec2 vec3::xy() const {
	return vec2(x,y);
}

float dot(vec3 a, vec3 b) {
	return a.x * b.x + a.y * b.y + a.z * b.z;
}
float length(vec3 v) {
	return sqrtf(v.x*v.x + v.y*v.y + v.z*v.z);
}
vec3 normalize(vec3 v) {
	float l = length(v);
	if (l > 0.f)
		return v / l;
	else
		return vec3(0.f);
}
vec3 maxvec(vec3 a, vec3 b) {
	return vec3(max(a.x, b.x), max(a.y, b.y), max(a.z, b.z));
}  
vec3 minvec(vec3 a, vec3 b) {
	return vec3(min(a.x, b.x), min(a.y, b.y), min(a.z, b.z));
}  
vec3 clampvec(vec3 v, vec3 c_min, vec3 c_max) {
	return minvec(maxvec(v, c_min), c_max);
}