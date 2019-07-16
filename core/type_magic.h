/* usage:

#define TYPE() MYTYPE
#define VALUES(X) \
X(one) \
X(two) \
X(three) 
DECLARE_ENUM(TYPE(), VALUES)
#undef VALUES
#undef TYPE

* _COUNT is appended automatically at the end
* can put the X macros in an include file then include it inline

*/

// using this madness
// http://stackoverflow.com/questions/147267/easy-way-to-use-variables-of-enum-types-as-string-in-c
// http://abissell.com/2014/01/16/c11s-_generic-keyword-macro-applications-and-performance-impacts/
// http://cecilsunkure.blogspot.com/2012/08/generic-programming-in-c.html
// https://gcc.gnu.org/onlinedocs/cpp/Concatenation.html
// http://stackoverflow.com/questions/1489932/c-preprocessor-and-token-concatenation

#include <string.h> // for strcmp

// expansion macro for enum value definition
#define ENUM_VALUE(name) TYPE()##_##name,

// expansion macro for enum to string conversion
#define ENUM_CASE(name) case TYPE()##_##name: return #name;

// expansion macro for string to enum conversion
#define ENUM_STRCMP(name) if (!strcmp(str,#name)) return name;

// expansion macro for count
#define ENUM_COUNT() TYPE()##_count,

#define PASTER(x,y) x ## _ ## NAME
#define PASTER2(x,y) x ## _ ## VALUE

/// declare the access function and define enum values
#define DECLARE_ENUM(EnumType,ENUM_DEF) \
	enum { \
	ENUM_DEF(ENUM_VALUE) \
	TYPE()##_count, \
	}; \
	inline const char* PASTER(EnumType,NAME)(int value) { \
    switch(value) \
	    { \
      ENUM_DEF(ENUM_CASE) \
      default: return "WHOW NELLY UNDEFINED ENUM!"; /* handle input error */ \
	    } \
	} \
	inline int PASTER2(EnumType,VALUE)(const char* str) { \
	for (int i = 0; i < TYPE()##_count; ++i) \
		if (strcmp(str, TYPE()##_NAME(i)) == 0) \
			return i; \
	return -1; \
	}

#if 0 //don't worry about declarations at the moment, just do it all inline
const char *GetString(EnumType dummy); \
EnumType Get##EnumType##Value(const char *string); \

/// define the access function names
#define DEFINE_ENUM(EnumType,ENUM_DEF) \
  const char *GetString(int value) \
  { \
    switch(value) \
    { \
      ENUM_DEF(ENUM_CASE) \
      default: return ""; /* handle input error */ \
    } \
  } \
  EnumType Get##EnumType##Value(const char *str) \
  { \
    ENUM_DEF(ENUM_STRCMP) \
    return (EnumType)0; /* handle input error */ \
  } \

///
#endif

//failed attempts, more typing, more replication


/*
#define X(x) TYPE()##_##x,

enum {
#include "test_enums.inc"
};

#undef X


const char* getName(int e) {
switch (e) {
#define X(x) case TYPE()##_##x: return #x; break;

#include "test_enums.inc"
default: return "UNDEFINED"; break;

#undef X

}
}
*/


