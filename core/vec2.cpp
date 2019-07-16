#include "vec2.h"
#include "core/maths.h"

ivec2::ivec2(const vec2& v) : x(int(v.x)), y(int(v.y)) { }
ivec2 maxvec(const ivec2 & a, const ivec2 & b) {
	return ivec2(max(a.x, b.x), max(a.y, b.y));
}
ivec2 minvec(const ivec2 & a, const ivec2 & b) {
	return ivec2(min(a.x, b.x), min(a.y, b.y));
}
ivec2 clampvec(ivec2 v, ivec2 c_min, ivec2 c_max) {
	return minvec(maxvec(v, c_min), c_max);
}
int taxilen(ivec2 v) {
	return abs(v.x) + abs(v.y); 
}

vec2::vec2(float s) : x(s), y(s) {}
vec2::vec2(float _x, float _y) : x(_x), y(_y) { }
vec2::vec2(ivec2 v) : x((float)v.x), y((float)v.y) { }

vec2 vec2::operator+(const vec2 & rhs) const {
	return vec2(x + rhs.x, y + rhs.y);
}

vec2 vec2::operator-(const vec2 & rhs) const {
	return vec2(x - rhs.x, y - rhs.y);
}

vec2 vec2::operator-() const {
	return vec2(-x, -y);
}

vec2 vec2::operator*(const float s) const {
	return vec2(x*s, y*s);
}

vec2 vec2::operator/(const float s) const {
	return vec2(x/s, y/s);
}

vec2 vec2::operator*(const vec2& rhs) const {
	return vec2(x*rhs.x, y*rhs.y);
}

vec2 vec2::operator/(const vec2& rhs) const {
	return vec2(x / rhs.x, y / rhs.y);
}

bool vec2::operator<(const vec2& rhs) const {
	return (x < rhs.x) && (y < rhs.y);
}
bool vec2::operator>(const vec2& rhs) const {
	return (x > rhs.x) && (y > rhs.y);
}
bool vec2::operator>=(const vec2& rhs) const {
	return (x >= rhs.x) && (y >= rhs.y);
}
bool vec2::operator<=(const vec2& rhs) const {
	return (x <= rhs.x) && (y <= rhs.y);
}
vec2 floor(vec2 v) {
	return vec2(floorf(v.x), floorf(v.y));
}
vec2 lerp(const vec2& a, const vec2& b, const vec2& t) {
	return vec2(lerp(a.x,b.x,t.x), lerp(a.y,b.y,t.y));
}
float length(const vec2 & v) {
	return sqrtf(v.x*v.x + v.y*v.y);
}
float lengthsqr(const vec2 & v) {
	return v.x*v.x + v.y*v.y;
}
vec2 perp(const vec2& v) {
	return vec2(-v.y,v.x);
}
vec2 normalize(const vec2 & v) {
	return v / length(v);
}
vec2 normalize_or_one(const vec2& v) {
	float l = length(v);
	return l > 0.0f ? v / l : vec2(1.0f, 0.0f);
}
vec2 normalize_or_one(const vec2& v, float* len) {
	float l = length(v);
	*len = l;
	return l > 0.0f ? v / l : vec2(1.0f, 0.0f);
}
vec2 normalize_or_zero(const vec2& v) {
	float l = length(v);
	return l > 0.0f ? v / l : vec2(0.0f, 0.0f);
}
vec2 rotate(const vec2 & r, const vec2 & v) {
	return vec2(v.x*r.x - v.y*r.y, v.x*r.y + v.y*r.x);
}

vec2 quat2d(float rad) {
	return vec2(cosf(rad), sinf(rad));
}
float torad(const vec2 v)
{
	return atan2f(v.y, v.x);
}
vec2 maxvec(const vec2 & a, const vec2 & b) {
	return vec2(max(a.x, b.x), max(a.y, b.y));
}

vec2 minvec(const vec2 & a, const vec2 & b) {
	return vec2(min(a.x, b.x), min(a.y, b.y));
}
float mincomp(const vec2& v) {
	return min(v.x,v.y);
}
vec2 clampvec(vec2 v, vec2 c_min, vec2 c_max) {
	return minvec(maxvec(v, c_min), c_max);
}
vec2 mulcomp(const vec2& a, const vec2& b) {
	return vec2(a.x*b.x, a.y*b.y);
}
float dot(const vec2& a, const vec2& b) {
	return a.y * b.y + a.x * b.x;
}
float det(const vec2& a, const vec2& b) {
	return a.x*b.y - a.y*b.x;
}
/*
vec2 lerp(const vec2 & a, const vec2 & b, float t)
{
	return a + (b - a) * t;
}
*/
