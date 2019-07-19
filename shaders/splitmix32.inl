uint SplitMix32(inout uint state) {
	uint b = (state += 0x9e3779b9);
	b ^= b >> 15;
	b *= 0x85ebca6b;
	b ^= b >> 13;
	b *= 0xc2b2ae3d;
	b ^= b >> 16;
	return b;
}
