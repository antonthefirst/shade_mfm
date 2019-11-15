
// Math

int taxilen(ivec2 v) { return abs(v.x) + abs(v.y); }

Atom _SITE_LOAD(ivec2 relative_idx) {
	if (taxilen(relative_idx) <= EVENT_WINDOW_RADIUS) {
		return imageLoad(img_site_bits, _SITE_IDX + relative_idx);
	} else {
		return new(Void);
	}
}
void _SITE_STORE(ivec2 relative_idx, Atom S) {
	if (taxilen(relative_idx) <= EVENT_WINDOW_RADIUS) {
		imageStore(img_site_bits, _SITE_IDX + relative_idx, S);
	}
}
void _SITE_SWAP(ivec2 a_idx, ivec2 b_idx) {
	Atom A = _SITE_LOAD(a_idx);
	Atom B = _SITE_LOAD(b_idx);
	if (_UNPACK_TYPE(A) != Void && _UNPACK_TYPE(B) != Void) {
		_SITE_STORE(a_idx, B);
		_SITE_STORE(b_idx, A);
	}
}

// EventWindow

C2D ew_getCoordRaw(SiteNum i) {
	switch (i) {
	case 0:  return ivec2( 0,  0);
	case 1:  return ivec2(-1,  0);
	case 2:  return ivec2( 0, -1);
	case 3:  return ivec2( 0, +1);
	case 4:  return ivec2(+1,  0);
	case 5:  return ivec2(-1, -1);
	case 6:  return ivec2(-1, +1);
	case 7:  return ivec2(+1, -1);
	case 8:  return ivec2(+1, +1);
	case 9:  return ivec2(-2,  0);
	case 10: return ivec2( 0, -2);
	case 11: return ivec2( 0, +2);
	case 12: return ivec2(+2,  0);
	case 13: return ivec2(-2, -1);
	case 14: return ivec2(-2, +1);
	case 15: return ivec2(-1, -2);
	case 16: return ivec2(-1, +2);
	case 17: return ivec2(+1, -2);
	case 18: return ivec2(+1, +2);
	case 19: return ivec2(+2, -1);
	case 20: return ivec2(+2, +1);
	case 21: return ivec2(-3,  0);
	case 22: return ivec2( 0, -3);
	case 23: return ivec2( 0,  3);
	case 24: return ivec2( 3,  0);
	case 25: return ivec2(-2, -2);
	case 26: return ivec2(-2, +2);
	case 27: return ivec2(+2, -2);
	case 28: return ivec2(+2, +2);
	case 29: return ivec2(-3, -1);
	case 30: return ivec2(-3, +1);
	case 31: return ivec2(-1, -3);
	case 32: return ivec2(-1, +3);
	case 33: return ivec2(+1, -3);
	case 34: return ivec2(+1, +3);
	case 35: return ivec2(+3, -1);
	case 36: return ivec2(+3, +1);
	case 37: return ivec2(-4,  0);
	case 38: return ivec2( 0, -4);
	case 39: return ivec2( 0, +4);
	case 40: return ivec2(+4,  0);
	default: return ivec2( 0,  0);
	};
}
C2D ew_mapSym(C2D c) {
	switch(_SYMMETRY) {
		case cSYMMETRY_000L: return C2D( c.x, c.y);
		case cSYMMETRY_090L: return C2D(-c.y, c.x);
		case cSYMMETRY_180L: return C2D(-c.x,-c.y);
		case cSYMMETRY_270L: return C2D( c.y,-c.x);
		case cSYMMETRY_000R: return C2D( c.x,-c.y);
		case cSYMMETRY_090R: return C2D( c.y, c.x);
		case cSYMMETRY_180R: return C2D(-c.x, c.y);
		case cSYMMETRY_270R: return C2D(-c.y,-c.x);
		default: return c; // not ever going to hit, but maybe help the compiler out...
	};
}
C2D ew_mapSym(SiteNum i) { return ew_mapSym(ew_getCoordRaw(i)); }

Atom ew(C2D idx) {
	return _SITE_LOAD(ew_mapSym(idx));
}
void ew(C2D idx, Atom S) {
	_SITE_STORE(ew_mapSym(idx), S);
}
Atom ew(SiteNum i) {
	return _SITE_LOAD(ew_mapSym(i));
}
void ew(SiteNum i, Atom S) {
	_SITE_STORE(ew_mapSym(i), S);
}
void ew_swap(C2D a, C2D b) {
	_SITE_SWAP(ew_mapSym(a), ew_mapSym(b));
}
void ew_swap(SiteNum a, SiteNum b) {
	_SITE_SWAP(ew_mapSym(a), ew_mapSym(b));
}
void ew_changeSymmetry(Symmetry s) {
	_SYMMETRY = s;
}
Symmetry ew_getSymmetry() {
	return _SYMMETRY;
}
bool ew_isLive(C2D c) {
	return _UNPACK_TYPE(_SITE_LOAD(c)) != Void;
}
bool ew_isLive(SiteNum n) {
	return ew_isLive(ew_mapSym(n));
}
bool ew_isEmpty(C2D c) {
	return _UNPACK_TYPE(_SITE_LOAD(c)) == Empty;
}
bool ew_isEmpty(SiteNum n) {
	return ew_isEmpty(ew_mapSym(n));
}

Bool ew_isLegal(C2D c) { return taxilen(c) <= EVENT_WINDOW_RADIUS; }
Bool ew_isLegal(SiteNum n) { return ew_isLegal(ew_mapSym(n)); }


// ColorUtils
ARGB cu_color(Byte red, Byte green, Byte blue) {
	return ARGB(255, red, green, blue);
}

ARGB cu_color(Unsigned col) {
	return ARGB((col >> 24) & 0xff, (col >> 16) & 0xff, (col >> 8) & 0xff, col & 0xff);
}

Atom _atoms[41];
int _nvotes_0;
int _nvotes_1;
int _nvotes_2;
int _nvotes_3;
int _nvotes_4;
int _nvotes_5;
int _nvotes_6;
int _nvotes_7;

SiteNum _winsn_0;
SiteNum _winsn_1;
SiteNum _winsn_2;
SiteNum _winsn_3;
SiteNum _winsn_4;
SiteNum _winsn_5;
SiteNum _winsn_6;
SiteNum _winsn_7;

Atom _winatom_0;
Atom _winatom_1;
Atom _winatom_2;
Atom _winatom_3;
Atom _winatom_4;
Atom _winatom_5;
Atom _winatom_6;
Atom _winatom_7;