
// site bit packing format:
// each site is a vector of 4 unsigned integers, x y z w

// X component:
// [0:7] - Type

// Y component:
// Z component:
// W component:

uint _UNPACK_TYPE(Atom A) { return (A[ATOM_TYPE_COMPONENT] >> ATOM_TYPE_LOCAL_OFFSET) & ATOM_TYPE_BITMASK; }
void _PACK_TYPE(inout Atom A, uint t) { A[ATOM_TYPE_COMPONENT] |= t << ATOM_TYPE_LOCAL_OFFSET; }

Atom new(uint type){
	Atom A = Atom(0);
	_PACK_TYPE(A, type);
	return A;
}