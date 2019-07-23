#pragma once
#include "basic_types.h"
#include "core/log.h" // for assert
#include <stdlib.h> // for realloc
#include <string.h> // for strlen

inline char toLower(char c) {
	if (c >= 'A' && c <= 'Z')
		c = c - 'A' + 'a';
	return c;
}

struct StringRange {
	const char* str;
	size_t len;
	inline StringRange() : str(NULL), len(0) { }
	inline StringRange(const char* s, size_t l) : str(s), len(l) { }
	inline StringRange(const char* s) {
		str = s;
		len = s ? strlen(s) : 0;
	}
	inline operator bool() {
		return str != NULL && len != 0;
	}
	inline bool operator==(const StringRange& rhs) {
		if (rhs.len != len) return false;
		if (len == 0) return true;
		const char* a = str;
		const char* b = rhs.str;
		do {
			if (*a != *b) return false;
			++a;
			++b;
		} while (a != (str + len));
		return true;
	}
	inline bool compareIgnoreCase(const StringRange& rhs) {
		if (rhs.len != len) return false;
		if (len == 0) return true;
		const char* a = str;
		const char* b = rhs.str;
		do {
			if (toLower(*a) != toLower(*b)) return false;
			++a;
			++b;
		} while (a != (str + len));
		return true;
	}
};
struct String {
	char* str = 0;
	size_t len = 0;
	size_t max_bytes = 0;
	inline void free() {
		::free(str);
		len = max_bytes = 0;
	}
	inline bool empty() const {
		return str == 0 || len == 0;
	}
	inline void clear() {
		len = 0;
	}
	inline void reservebytes(size_t b) {
		if (b > max_bytes) {
			char* mem = (char*)realloc(str, b);
			assert(mem);
			if (mem) {
				str = mem;
				max_bytes = b;
			}
		}
	}
	inline void setlen(size_t l) {
		len = l;
	}
	inline void append(const char* s, size_t l) {
		reservebytes(len + l + 1);
		assert(max_bytes > (len + l));
		if (max_bytes > (len + l)) {
			memcpy(str + len, s, l);
			len += l;
			str[len] = 0;
		}
	}
	inline void append(const char* s) {
		while (char c = *s++) {
			if (len + 1 >= max_bytes) { reservebytes((len + 1) * 2); }
			assert(str);
			if (!str) { len = 0; return; }
			if (len + 1 < max_bytes) str[len] = c;
			++len;
		}
		if (len > 0)
			str[len] = 0;
	}
	inline void append(StringRange r) { append(r.str, r.len);  }
	inline void set(const char* s, size_t l) {
		reservebytes(l + 1);
		assert(max_bytes > l);
		if (max_bytes > l) {
			memcpy(str, s, l);
			len = l;
			str[len] = 0;
		}
	}
	inline void set(const char* s) {
		len = 0;
		append(s);
	}
	inline void set(StringRange r) {
		set(r.str, r.len);
	}
	inline StringRange range() const {
		return StringRange(str, len);
	}
};
