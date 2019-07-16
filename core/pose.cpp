#include "pose.h"
#include "maths.h"
pose identity() { return pose(0, 0, 1, 0); }
pose inverse(pose t) {
	float s = lengthsqr(t.zw());
	return pose(0,0, t.z / s, -t.w / s) * pose(-t.x, -t.y, 1, 0);
}
pose normalize(pose t) {
	vec2 sr = normalize_or_one(t.zw());
	return pose(t.x, t.y, sr.x, sr.y);
}
pose rotation(float rad)
{
	return pose(0,0, cosf(rad), sinf(rad));
}
pose scale(float s)
{
	return pose(0,0,s,0);
}
pose trans(vec2 pos, float rot, float scale)
{
	return pose(pos.x, pos.y, scale*cosf(rot), scale*sinf(rot));
}
float scaleof(pose t) {
	return length(t.zw());
}
pose operator*(pose lhs, pose rhs)
{
	vec2 p = lhs.xy() + rotate(lhs.zw(), rhs.xy());
	vec2 r = rotate(lhs.zw(), rhs.zw());
	return pose(p.x, p.y, r.x, r.y);
}
vec2 operator*(pose lhs, vec2 rhs)
{
	return lhs.xy() + rotate(lhs.zw(), rhs);
}
pose operator~(pose uno)
{
	return inverse(uno);
}
