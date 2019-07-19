
uint random_bits(uint bitsize) {
	uint bits = XoroshiroNext32();
	return bitsize == 32 ? bits : bits & ((1 << bitsize) - 1);
}

// range [0, max), maximum MAX is UINT_MAX
uint random_create(uint val_max) {
	// this code is adapted from: http://www.pcg-random.org, basic generator C file (http://www.pcg-random.org/downloads/pcg-c-basic-0.9.zip)
	// the loop is guaranteed to terminate if the generator is uniform
	// the chance of this loop re-rolling is roughly proportional to the range
	// for small ranges up to say 1024, the chance is around 0.00002%, or 1 in 4 million.
	// for medium ranges of say 2^16, the changes is around 0.002% or 1 in 66 thousand.
	// so it's safe to say for "most" cases, it will return without actually looping.
    uint threshold = -val_max % val_max;
    for (;;) {
        uint r = XoroshiroNext32();
        if (r >= threshold)
            return r % val_max;
    }
}
int random_create(int val_max) {
	if (val_max <= 0) return 0;
	return int(random_create(uint(val_max)));
}

// range [min, max], maximum MAX is UINT_MAX-1
int random_between(int lo, int hi) {
	if (lo > hi) {
      int t = hi;
      hi = lo;
      lo = t;
	}
	return lo + int(random_create(hi - lo + 1));
}

bool random_oddsOf(uint thisMany, uint outOfThisMany) {
	return random_create(outOfThisMany) < thisMany;
}
bool random_oddsOf(int thisMany, int outOfThisMany) {
	return int(random_create(outOfThisMany)) < thisMany;
}
bool random_oneIn(uint odds) {
	return random_oddsOf(uint(1), odds);
}