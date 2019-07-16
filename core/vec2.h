#pragma  once
struct vec2;

struct ivec2 {
	int x, y;
	ivec2() { }
	ivec2(int _v) : x(_v), y(_v) { }
	ivec2(int _x, int _y) : x(_x), y(_y) { }
	explicit ivec2(const vec2& v);
	bool operator==(ivec2 rhs) const { return x == rhs.x && y == rhs.y; }
	bool operator!=(ivec2 rhs) const { return !operator==(rhs); }
	bool operator<(ivec2 rhs) const { return x < rhs.x && y < rhs.y; }
	bool operator<=(ivec2 rhs) const { return x <= rhs.x && y <= rhs.y; }
	bool operator>(ivec2 rhs) const { return x > rhs.x && y > rhs.y; }
	bool operator>=(ivec2 rhs) const { return x >= rhs.x && y >= rhs.y; }
	ivec2 operator+(ivec2 rhs) const { return ivec2(x + rhs.x, y + rhs.y); }
	ivec2 operator-(ivec2 rhs) const { return ivec2(x - rhs.x, y - rhs.y); }
	ivec2 operator/(ivec2 rhs) const { return ivec2(x / rhs.x, y / rhs.y); }
	ivec2 operator*(ivec2 rhs) const { return ivec2(x * rhs.x, y * rhs.y); }
	void operator+=(ivec2 rhs) { x += rhs.x; y += rhs.y; }
	void operator-=(ivec2 rhs) { x -= rhs.x; y -= rhs.y; }
};
inline ivec2 wrap(ivec2 p, ivec2 w) {
	if (p.x >= w.x) p.x = p.x - w.x;
	if (p.x < 0) p.x = w.x + p.x;
	if (p.y >= w.y) p.y = p.y - w.y;
	if (p.y < 0) p.y = w.y + p.y;
	return p;
}
ivec2 maxvec(const ivec2& a, const ivec2& b);
ivec2 minvec(const ivec2& a, const ivec2& b);
ivec2 clampvec(ivec2 v, ivec2 c_min, ivec2 c_max);
int taxilen(ivec2 v);


struct vec2
{
	float x, y;
	vec2() { }
	explicit vec2(float s);
	vec2(float x, float y);
	explicit vec2(ivec2 v);
	vec2 operator+(const vec2& rhs) const;
	vec2 operator-(const vec2& rhs) const;
	vec2 operator-() const;
	vec2 operator*(const vec2& rhs) const;
	vec2 operator/(const vec2& rhs) const;
	bool operator<(const vec2& rhs) const;
	bool operator>(const vec2& rhs) const;
	bool operator>=(const vec2& rhs) const;
	bool operator<=(const vec2& rhs) const;
	vec2 operator*(const float s) const;
	vec2 operator/(const float s) const;
	inline void operator-=(const vec2& rhs) { *this = *this - rhs; }
	inline void operator+=(const vec2& rhs) { *this = *this + rhs; }
	inline void operator/=(const float s) { *this = *this / s; }
	inline void operator*=(const float s) { *this = *this * s; }
	inline bool operator==(const vec2& rhs) { return x == rhs.x && y == rhs.y; }
	inline bool operator!=(const vec2& rhs) { return !operator==(rhs); }
};

vec2 floor(vec2 v);
vec2 lerp(const vec2& a, const vec2& b, const vec2& t);
float length(const vec2 & v);
float lengthsqr(const vec2 & v);
vec2 perp(const vec2& v);
vec2 normalize(const vec2 & v);
vec2 normalize_or_one(const vec2& v);
vec2 normalize_or_one(const vec2& v, float* len);
vec2 normalize_or_zero(const vec2& v);
vec2 rotate(const vec2& r, const vec2& v);
vec2 quat2d(float rad);
float torad(const vec2 v);
vec2 maxvec(const vec2& a, const vec2& b);
vec2 minvec(const vec2& a, const vec2& b);
float mincomp(const vec2& v);
vec2 clampvec(vec2 v, vec2 c_min, vec2 c_max);
vec2 mulcomp(const vec2& a, const vec2& b);
float dot(const vec2& a, const vec2& b);
float det(const vec2& a, const vec2& b);
//vec2 lerp(const vec2& a, const vec2& b, float t);

