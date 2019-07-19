// keep this small! vague guidelines of what belongs here:
// * basic commonly used formulas (on par with pow, exp etc)
// * basic linear algebra ops (on par with matrix mul, cross, dot etc)
// * no "algorithms" or "procedures"

#define PI    3.14159265358979323846
#define TAU (PI*2.0)
#define EXP   2.71828182845904523536
#define SQRT3 1.73205080757
#define PHI   1.61803398874989484820
#define PHI2 (1.61803398874989484820*1.61803398874989484820)
#define GOLDEN_RATIO PHI
#define GOLDEN_ANGLE 2.39996322972865332223 // (TAU/PHI2) https://oeis.org/A131988

#define FLT_MIN 1.175494351e-38
#define FLT_MAX 3.402823466e+38

/*** One-liners ***/
float saturate(float x) { return clamp(x, 0.0, 1.0); }
vec2 saturate(vec2 x) { return clamp(x, vec2(0.0), vec2(1.0)); }
vec3 saturate(vec3 x) { return clamp(x, vec3(0.0), vec3(1.0)); }
vec4 saturate(vec4 x) { return clamp(x, vec4(0.0), vec4(1.0)); }
float sq(float x) { return x * x; }
float step1(float x) { return x > 0.0 ? 1.0 : 0.0; }
float maxcomp(vec3 v) { return max(v.x, max(v.y, v.z)); }
float maxcomp(vec2 v) { return max(v.x, v.y); }
float mincomp(vec3 v) { return min(v.x, min(v.y, v.z)); }
float mincomp(vec2 v) { return min(v.x, v.y); }
float sgn(float x) { return (x < 0) ? -1 : 1; } // Sign function that doesn't return 0
vec2  sgn(vec2 v) { return vec2((v.x < 0) ? -1 : 1, (v.y < 0) ? -1 : 1); }
float saw(float t) { return 1.0 - abs(fract(t*0.5) * 2.0 - 1.0); }
float dot2(in vec2 v) { return dot(v, v); }
float dot2(in vec3 v) { return dot(v, v); }
float lengthsq(in vec2 v) { return dot(v, v); }
float lengthsq(in vec3 v) { return dot(v, v); }
vec2 perp(in vec2 v) { return vec2(-v.y, v.x); }
float sum(vec2 v) { return v.x + v.y; }
float sum(vec3 v) { return v.x + v.y + v.z; }
float sum(vec4 v) { return v.x + v.y + v.z + v.w; }

/*** Shaping functions ***/
// From old renderman times. x [0,1], k (0,1) Warning, unstable near 0 and 1
float bias(float x, float b) { return  x / ((1.0 / b - 2.0)*(1.0 - x) + 1.0); }
vec2  bias(vec2 v,  vec2 b)  { return vec2(bias(v.x, b.x), bias(v.y, b.y)); }
float gain(float x, float g) { float t = (1.0 / g - 2.0)*(1.0 - (2.0*x)); return x<0.5 ? (x / (t + 1.0)) : (t - x) / (t - 1.0); }
vec2  gain(vec2 v,  vec2 g)  { return vec2(gain(v.x,g.x), gain(v.y,g.y)); }
// from Dino Dini. x [-1,+1], k (-1, +1) https://dinodini.wordpress.com/2010/04/05/normalized-tunable-sigmoid-functions/
float dino(float x, float k) { return (x - k*x) / (k - 2.0*k*abs(x) + 1.0); }
float dinostep01(float x, float k) { return dino(x*2.0 - 1.0, k)*0.5 + 0.5; }
vec2  dinostep01(vec2  v, vec2  k) { return vec2(dinostep01(v.x, k.x), dinostep01(v.y, k.y)); }

/*** Image coordinate conversions ***/
vec2 st_from_id(in uvec2 id) { return  vec2(id) + vec2(0.5); }
vec2 st_from_id(in ivec2 id) { return  vec2(id) + vec2(0.5); }
ivec2 id_from_st(in vec2 st) { return ivec2(st); }
ivec2 id_from_uv(in vec2 uv, in vec2 res) { return ivec2(uv * res); }
vec2 xy_from_st(in vec2 st, in vec2 res) { return (2.0 * st - res) / res.y; }
vec2 st_from_xy(in vec2 xy, in vec2 res) { return ((xy * res.y) + res) * 0.5; }
vec2 uv_from_st(in vec2 st, in vec2 res) { return st / res; }
vec2 uv_from_id(in ivec2 id, in vec2 res) { return (vec2(id) + vec2(0.5)) / res; }
vec2 st_from_uv(in vec2 uv, in vec2 res) { return uv * res; }

/*** Image addressing ***/
vec4 image(sampler2D tex, vec2 st) { return texture(tex, uv_from_st(st, textureSize(tex, 0))); }

/*** Basic color conversions from http://lolengine.net/blog/2013/07/27/rgb-to-hsv-in-glsl ***/
vec3 rgb_from_hsv(vec3 c) {
	vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
	vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
	return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}
vec3 hsv_from_rgb(vec3 c) {
	vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
	vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
	vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));

	float d = q.x - min(q.w, q.y);
	float e = 1.0e-10;
	return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}
float srgb_from_lrgb(float val) {
	if (val < 0.0031308)
		val *= 12.92f;
	else
		val = 1.055f * pow(val, 1.0f / 2.4f) - 0.055f;
	return val;
}
float lrgb_from_srgb(float val) {
	if (val < 0.04045)
		val /= 12.92f;
	else
		val = pow((val + 0.055f) / 1.055f, 2.4f);
	return val;
}
vec3 srgb_from_lrgb(vec3 col) { return vec3(srgb_from_lrgb(col.x), srgb_from_lrgb(col.y), srgb_from_lrgb(col.z)); }
vec3 lrgb_from_srgb(vec3 col) { return vec3(lrgb_from_srgb(col.x), lrgb_from_srgb(col.y), lrgb_from_srgb(col.z)); }


/*** 2d rotations ***/
vec2 rotation_2d(float a) { return vec2(cos(a), sin(a)); }
vec2 rotate(vec2 r, vec2 v) { return vec2(v.x*r.x - v.y*r.y, v.x*r.y + v.y*r.x); }
vec2 rotate(float r, vec2 v) { return rotate(rotation_2d(r), v); }

/*** Quaternion ops ***/
vec4 quat() { return vec4(0.0, 0.0, 0.0, 1.0); }
vec4 quat(vec3 axis, float angle) { return vec4(axis*sin(angle*0.5), cos(angle*0.5)); }
vec4 quat_conj(vec4 q) { return vec4(-q.xyz, q.w); }
vec4 quat_mul(vec4 q1, vec4 q2) {
	return vec4(
		q1.w*q2.x + q1.x*q2.w + q1.y*q2.z - q1.z*q2.y,
		q1.w*q2.y - q1.x*q2.z + q1.y*q2.w + q1.z*q2.x,
		q1.w*q2.z + q1.x*q2.y - q1.y*q2.x + q1.z*q2.w,
		q1.w*q2.w - q1.x*q2.x - q1.y*q2.y - q1.z*q2.z);
}
// this appears to be the fastest, oddly (it's more ops than the below, but maybe due to instruction limit reuse is better?)
// update: it appears that another one is faster now...maybe depends on how register bound we are?
//vec3 quat_mul(vec4 q, vec3 v) { return quat_mul(quat_mul(q, vec4(v, 0.0)), quat_conj(q)).xyz; }
//vec3 quat_mul(vec4 q, vec3 v) { return 2.0 * dot(q.xyz, v) * q.xyz + (q.w*q.w - dot(q.xyz, q.xyz)) * v + 2.0 * q.w * cross(q.xyz, v); }
vec3 quat_mul(vec4 q, vec3 v) { return v + 2.0*cross(q.xyz, cross(q.xyz, v) + q.w*v); }
//vec3 quat_mul(vec4 q, vec3 v) { return v * (q.w*q.w - dot(q.xyz, q.xyz)) + 2.0*q.xyz*dot(q.xyz, v) + 2.0*q.w*cross(q.xyz, v); }
vec4 quat(vec3 rot) { // from 3 consecutive rotations, in order x,y,z (if interpreted as world_from_quat)
	vec4 qx = vec4(vec3(sin(rot.x * 0.5f), 0, 0), cos(rot.x * 0.5f));
	vec4 qy = vec4(vec3(0, sin(rot.y * 0.5f), 0), cos(rot.y * 0.5f));
	vec4 qz = vec4(vec3(0, 0, sin(rot.z * 0.5f)), cos(rot.z * 0.5f));
	return quat_mul(qz, quat_mul(qy, qx));
}


/*** Pose ops ***/
vec3 pose_mul(vec3 pos, vec4 rot, vec3 v) { return pos + quat_mul(rot, v); }
void pose_inverse(inout vec3 pos, inout vec4 rot) {
	rot = quat_conj(rot);
	pos = quat_mul(rot, -pos);
}
 