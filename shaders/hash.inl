uint uint_hash(uint x) {
    x = (x ^ 61U) ^ (x >> 16U);
    x *= 9U;
    x = x ^ (x >> 4U);
    x *= 0x27d4eb2dU;
    x = x ^ (x >> 15U);
    return x;
}
uint uint_hash(uvec2 x) {
	return uint_hash(uint_hash(x.x) ^ x.y);
}
uint uint_prng(uint b) {
	b ^= b << 13;
	b ^= b >> 17;
	b ^= b << 5;
	return b;
}
uint uint_hash(uvec3 b) {
	return uint_hash(uint_hash(uint_hash(b.x) ^ b.y) ^ b.z);
}

uint uint_hash(float p) { return uint_hash(floatBitsToUint(p)); }
uint uint_hash(vec2  p) { return uint_hash(floatBitsToUint(p)); }
uint uint_hash(vec3  p) { return uint_hash(floatBitsToUint(p)); }

uint hash(vec2 uv, uint time_salt) {
    uvec2 bits = floatBitsToUint(uv);
    return uint_hash(uint_hash(bits) ^ time_salt);
}

float unorm_from(uint  b) { return float(b) *     (1.0/4294967295.0); }
vec2  unorm_from(uvec2 b) { return  vec2(b) * vec2(1.0/4294967295.0); }
vec3  unorm_from(uvec3 b) { return  vec3(b) * vec3(1.0/4294967295.0); }

vec3 unorm3_from(uint b) {
    uint bb = uint_prng(b);
    return unorm_from(uvec3(b,bb,uint_prng(bb)));
}
vec3 color_from_bits(uint v) {
	return unorm3_from(v);
}