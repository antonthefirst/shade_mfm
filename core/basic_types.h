#ifndef _header_basic_types_
#define _header_basic_types_
#include <stdint.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t  s8;
typedef int16_t s6;
typedef int32_t s32;
typedef int64_t s64;

typedef float  f32;
typedef double f64;

static_assert( sizeof(u8)==1,  "u8 of wrong size");
static_assert(sizeof(u16)==2, "u16 of wrong size");
static_assert(sizeof(u32)==4, "u32 of wrong size");
static_assert(sizeof(u64)==8, "u64 of wrong size");

#endif
