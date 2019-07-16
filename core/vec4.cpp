#include "vec4.h"
#include "maths.h"

vec4::vec4(float _s) : x(_s), y(_s), z(_s), w(_s) { }
vec4::vec4(float _x, float _y, float _z, float _w) : x(_x), y(_y), z(_z), w(_w) {}
vec4::vec4(const vec2 & v, float _z, float _w) : x(v.x), y(v.y), z(_z), w(_w) {}
vec4::vec4(const vec3 & v, float _w) : x(v.x), y(v.y), z(v.z), w(_w) {}
vec4::vec4(const vec2& xy, const vec2& zw) : x(xy.x), y(xy.y), z(zw.x), w(zw.y) {}
vec4 vec4::operator+(const vec4 & rhs) {
	return vec4(x + rhs.x, y + rhs.y, z + rhs.z, w + rhs.w);
}
vec4 vec4::operator-(const vec4 & rhs) {
	return vec4(x - rhs.x, y - rhs.y, z - rhs.z, w - rhs.w);
}
vec4 vec4::operator*(const float s) {
	return vec4(x*s, y*s, z*s, w*s);
}
vec4 vec4::operator/(const float s) {
	return vec4(x / s, y / s, z / s, w / s);
}
bool vec4::operator==(const vec4& rhs) {
	return x == rhs.x && y == rhs.y && z == rhs.z && w == rhs.w;
}
vec2 vec4::xy() const {
	return vec2(x, y);
}
vec3 vec4::xyz() const {
	return vec3(x, y, z);
}
vec2 vec4::zw() const
{
	return vec2(z, w);
}
void vec4::xy(vec2 new_xy) {
	x = new_xy.x;
	y = new_xy.y;
}
void vec4::zw(vec2 new_zw) {
	z = new_zw.x;
	w = new_zw.y;
}
vec4 operator+(const vec4 & lhs, const vec4 & rhs) {
	return vec4(lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z, lhs.w + rhs.w);
}
vec4 operator-(const vec4 & lhs, const vec4 & rhs) {
	return vec4(lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z, lhs.w - rhs.w);
}
vec4 lerp(const vec4 & a, const vec4 & b, float t) {
	return a + (b - a)*t;
}
vec4 maxvec(vec4 a, vec4 b) {
	return vec4(max(a.x, b.x), max(a.y, b.y), max(a.z, b.z), max(a.w, b.w));
}  
vec4 minvec(vec4 a, vec4 b) {
	return vec4(min(a.x, b.x), min(a.y, b.y), min(a.z, b.z), min(a.w, b.w));
}  
vec4 clampvec(vec4 v, vec4 c_min, vec4 c_max) {
	return minvec(maxvec(v, c_min), c_max);
}