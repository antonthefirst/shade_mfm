// mfm constants
#define EVENT_WINDOW_RADIUS 4

// event window
#define Symmetry uint
#define cSYMMETRY_000L (0u)
#define cSYMMETRY_090L (1u)
#define cSYMMETRY_180L (2u)
#define cSYMMETRY_270L (3u)
#define cSYMMETRY_000R (4u)
#define cSYMMETRY_090R (5u)
#define cSYMMETRY_180R (6u)
#define cSYMMETRY_270R (7u)

// internal datatypes
#define EventWindow uint
#define PrngState uint
#define PrngOutput uint

// basic datatypes
#define Atom     uvec4
#define AtomType uint
#define ARGB     uvec4

#define C2D      ivec2
#define Unsigned uint
#define Int      int
#define Byte     uint
#define Bool     bool

#define SiteNum uint

#define XoroshiroState uvec4

#define InvalidSiteNum (63)
#define InvalidAtom (Atom(0))

uint XoroshiroNext32();
