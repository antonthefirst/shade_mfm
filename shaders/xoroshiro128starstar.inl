// from http://xoshiro.di.unimi.it/xoshiro128starstar.c

uint xororotl(uint x, int k) {
	return (x << k) | (x >> (32 - k));
}
XoroshiroState xoroshiro128_unpack(uvec4 v) {
	return v;
}
uvec4 xoroshiro128_pack(XoroshiroState X) {
	return X;
}
uint XoroshiroNext32() {
	uint result_starstar = xororotl(_XORO[0] * 5, 7) * 9;
	uint t = _XORO[1] << 9;

	_XORO[2] ^= _XORO[0];
	_XORO[3] ^= _XORO[1];
	_XORO[1] ^= _XORO[2];
	_XORO[0] ^= _XORO[3];

	_XORO[2] ^= t;

	_XORO[3] = xororotl(_XORO[3], 11);

	return result_starstar;
}
