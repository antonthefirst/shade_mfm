#ifndef _container_h_
#define _container_h_
#include <initializer_list>
#include "memory.h"
#include "core/log.h" //for assertm
#include "maths.h" //for max
#include "core/basic_types.h"
#include <new>

#if NDEBUG
#define CONTAINER_CHECK_NULL 0
#define CONTAINER_CHECK_BOUNDS 0
#else
#define CONTAINER_CHECK_NULL 1
#define CONTAINER_CHECK_BOUNDS 1
#endif

#if CONTAINER_CHECK_BOUNDS
#define CHECK_BOUNDS(i, m) assertm((i >= 0 && i < m), "Index out of bounds %lld / %lld", i,m)
#else
#define CHECK_BOUNDS(i, m)
#endif

#if CONTAINER_CHECK_NULL
#define CHECK_NULL(p) assertm(p, "Container pointer is null")
#else
#define CHECK_NULL(p)
#endif

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;


// a bunch of objects, unordered
// fast append and erase
// id's are not stable on erase
// pointers are not stable on append or erase (due to potential resize in append)
template<typename T>
struct Bunch {
	T* ptr = 0;
	int64_t maxcount = 0;
	int64_t count = 0;

	Bunch() { }
	~Bunch() {
		free(ptr);
	}
	Bunch(const Bunch& rhs) {
		operator=(rhs);
	}
	Bunch(std::initializer_list<T> list) {
		clear();
		reserve(list.size());
		for (auto i = list.begin(); i != list.end(); ++i)
			push(*i);
	}
	Bunch& operator=(const Bunch& rhs) {
		clear();
		reserve(rhs.count);
		for (int i = 0; i < rhs.count; ++i)
			push(rhs[i]);
		return *this;
	}
	Bunch& operator=(std::initializer_list<T> list) {
		clear();
		reserve(list.size());
		for (auto i = list.begin(); i != list.end(); ++i)
			push(*i);
		return *this;
	}
	Bunch& copypod(const Bunch& rhs) { // copy if the contents are plain old data
		reserve(rhs.count);
		count = rhs.count;
		memcpy(ptr, rhs.ptr, size_t(rhs.bytes())); //64bit
		return *this;
	}
	void clear() {
		count = 0;
	}
	bool empty() const {
		return count == 0;
	}
	void reserve(int64_t size) {
		if (size > maxcount) {
			ptr = (T*)realloc(ptr, size_t(size * sizeof(T))); //64bit
			assertm(ptr, "Out of memory! %p", ptr);
			maxcount = size;
		}
	}
	void setgarbage(int64_t size) {
		reserve(size);
		count = size;
	}
	void push(const T& t) {
		if (count >= maxcount)
			reserve((2 * max(int64_t(2), maxcount)));
		CHECK_NULL(ptr);
		CHECK_BOUNDS(count, maxcount);
		ptr[count++] = t;
	}
	T& push() {
		if (count >= maxcount)
			reserve((2 * max(int64_t(2), maxcount)));
		CHECK_NULL(ptr);
		CHECK_BOUNDS(count, maxcount);
		new(ptr+(count++)) T;
		return ptr[count - 1];
	}
	T& pushgarbage() {
		if (count >= maxcount)
			reserve((2 * max(int64_t(2), maxcount)));
		CHECK_NULL(ptr);
		CHECK_BOUNDS(count, maxcount);
		count++;
		return ptr[count - 1];
	}
	void pushi(int64_t num) {
		reserve(num);
		for (num; num > 0; --num)
			push();
	}
	void pushi(int64_t num, const T& val) {
		reserve(num);
		for (num; num > 0; --num)
			push(val);
	}
	void pushunique(const T& t) {
		for (int i = 0; i < count; ++i)
			if (operator[](i) == t)
				return;
		push(t);
	}
	T* find(const T& target) {
		for (int64_t i = 0; i < count; ++i)
			if (ptr[i] == target)
				return &ptr[i];
		return 0;
	}
	void insert(int64_t i, const T& val) {
		if (count >= maxcount)
			reserve((2 * max(int64_t(2), maxcount)));
		CHECK_NULL(ptr);
		CHECK_BOUNDS(count, maxcount);
		assert(count-i >= 0);
		memmove(ptr+i+1, ptr+i, (count-i)*sizeof(T));
		ptr[i] = val;
		count++;
	}
	/*void findremove(const T& target) {
		for (int64_t i = 0; i < count; ++i)
			if (ptr[i] == target)
				remove(i--);
	}*/
	void copy(const Bunch<T>& other) {
		reserve(other.count);
		count = other.count;
		memcpy(ptr, other.ptr, other.bytes());
	}
	void pop() {
		CHECK_NULL(ptr);
		CHECK_BOUNDS(count-1, count);
		count = max(int64_t(0), count - 1);
	}
	T& top() {
		CHECK_NULL(ptr);
		CHECK_BOUNDS(count-1, count);
		return ptr[count - 1];
	}
	T poptop() {
		T ret = top();
		pop();
		return ret;
	}
	void erase(int64_t i) {
		CHECK_NULL(ptr);
		CHECK_BOUNDS(count-1, count);
		assert((count-i-1) >= 0);
		memmove(ptr+i, ptr+i+1, (count-i-1)*sizeof(T));
		count = max(int64_t(0), count - 1);
	}
	void remove(int64_t i) {
		CHECK_NULL(ptr);
		CHECK_BOUNDS(i, count);
		if (i != count - 1)
			ptr[i] = top();
		count = max(int64_t(0), count - 1);
	}
	T& operator[](int64_t i) {
		CHECK_NULL(ptr);
		CHECK_BOUNDS(i, count);
		return ptr[i];
	}
	const T& operator[](int64_t i) const {
		CHECK_NULL(ptr);
		CHECK_BOUNDS(i, count);
		return ptr[i];
	}
	int64_t bytes() const {
		return count*sizeof(T);
	}
	T* end() const {
		return ptr + count;
	}
};

template<typename T, s64 MAX_COUNT>
struct Pipe {
	const s64 maxcount = MAX_COUNT;

	T ptr[MAX_COUNT];
	s64 fi = 0;
	s64 bi = 0;
	s64 count = 0;

	Pipe() { }
	inline void clear() {
		fi = bi = count = 0;
	}
	inline void push(const T& t) {
		if (count < maxcount) {
			ptr[bi] = t;
			bi = (bi + 1) % maxcount;
			++count;
		} else {
			log("DEQUE ERROR: Overflow\n");
		}
	}
	inline void pushunique(const T& t) {
		for (int i = 0; i < count; ++i)
			if (operator[](i) == t)
				return;
		push(t);
	}
	inline T pop() {
		if (count > 0) {
			T ret = ptr[fi];
			--count;
			fi = (fi + 1) % maxcount;
			return ret;
		} else {
			T ret = ptr[fi];
			log("DEQUE ERROR: Underflow\n");
			return ret;
		}
	}
	inline T& front() {
		if (count > 0) {
			return ptr[fi];
		} else {
			log("DEQUE ERROR: No front\n");
			return ptr[0]; //suppress warning
		}
	}
	inline T& back() {
		if (count > 0) {
			return ptr[(bi - 1 + maxcount) % maxcount];
		} else {
			log("DEQUE ERROR: No back\n");
			return ptr[0]; //suppress warning
		}
	}
	inline bool full() {
		return count == maxcount;
	}
	inline bool hasroomfor(int num) {
		return (count + num) <= maxcount;
	}
	inline bool empty() {
		return count == 0;
	}
	inline void set(const T& t) {
		clear();
		for (s64 i = 0; i < maxcount; ++i)
			ptr[i] = t;
		count = maxcount;
	}
	inline T& operator[](s64 i) {
		if (i >= 0 && i < count) {
			return ptr[(fi + i) % maxcount];
		} else {
			log("DEQUE ERROR: Index out of bounds\n");
			return ptr[bi];
		}
	}
};

template<typename T, int MAX_COUNT>
struct Bin {
	static const int32_t maxcount = MAX_COUNT;

	T ptr[MAX_COUNT];
	int32_t count = 0;

	Bin() {}
	Bin(std::initializer_list<T> list) {
		for (auto i = list.begin(); i != list.end(); ++i) {
			if (count == maxcount)
				break;
			push(*i);
		}
	}
	void set(const T& t) {
		setgarbage();
		for (int i = 0; i < count; ++i)
			ptr[i] = t;
	}
	void set(std::initializer_list<T> list) {
		count = 0;
		for (auto i = list.begin(); i != list.end(); ++i) {
			if (count == maxcount)
				break;
			push(*i);
		}
	}
	void setgarbage(int32_t num = MAX_COUNT) {
		count = num;
	}
	void push(const T& t) {
		if (count == maxcount) {
			log("WARNING: Bin is full!\n");
			assertm(count < maxcount, "Bin overflow");
			return;
		}
		ptr[count++] = t;
	}
	T& push() {
		if (count < maxcount) {
			new(ptr+(count++)) T;
		} else {
			log("WARNING: Bin is full!\n");
			assertm(count < maxcount, "Bin overflow");
		}
		return ptr[count-1];
	}
	inline void pushunique(const T& t) {
		for (int i = 0; i < count; ++i)
			if (operator[](i) == t)
				return;
		push(t);
	}
	void pushi(const T& t, int32_t num) {
		for (int i = 0; i < num; ++i)
			push(t);
	}
	void pop() {
		if (count == 0) {
			log("WARNING: Bin is empty!\n");
			assertm(count > 0, "Bin underflow");
			return;
		}
		count = max(0, count - 1);
	}
	T& top() {
		return ptr[count - 1];
	}
	void remove(int32_t i) {
		if (i != count - 1)
			ptr[i] = top();
		pop();
	}
	void remove_ordered(int32_t i) {
		CHECK_BOUNDS(i, count);
		if (count > 1 && (i < count - 1)) { //we've got more than 1 element, and it's not the last element
			memcpy(&ptr[i], &ptr[i + 1], (count - 1 - i)*sizeof(T));
		}
		--count;
	}
	void clear() {
		count = 0;
	}
	bool empty() {
		return count == 0;
	}
	bool full() {
		return count == maxcount;
	}
	T& operator[](int64_t i) {
		CHECK_BOUNDS(i, count);
		return ptr[i];
	}
	const T& operator[](int64_t i) const {
		CHECK_BOUNDS(i, count);
		return ptr[i];
	}
	size_t bytes() {
		return count*sizeof(T);
	}
};

template<typename T, int MAX_COUNT>
struct StaticArray {
	T ptr[MAX_COUNT];
	static const int max_count = MAX_COUNT;
	int count;
	StaticArray() {
		count = 0;
	}
	void resize(int _count) {
		count = _count;
	}
	void fill(const T& t) {
		for (int i = 0; i < count; ++i)
			ptr[i] = t;
	}
	void push(const T& d) {
		assertm(count < max_count, "Static Stack overflow");
		ptr[count++] = d;
	}
	void pop() {
		assertm(count > 0, "Static Stack underflow");
		--count;
	}
	void clear() {
		count = 0;
	}
	T& operator[](int i) {
		CHECK_BOUNDS(i, count);
		return ptr[i];
	}
	const T& operator[](int i) const {
		CHECK_BOUNDS(i, count);
		return ptr[i];
	}

	T& last() { return ptr[count-1]; }
	const T& last() const { return ptr[count-1]; }

	size_t bytes() { return count*sizeof(T); }
};

template<typename T>
struct Array {
	T* ptr;
	int count;
	Array() {
		ptr = 0;
		count = 0;
	}
	void alloc(int _count) {
		ptr = (T*)malloc(_count*sizeof(T));
		assertm(ptr, "Out of memory! %p", ptr);
		count = _count;
	}
	void free() {
		memFree(ptr);
		ptr = 0;
		count = 0;
	}
	void fill(const T& t) {
		for (int i = 0; i < count; ++i)
			ptr[i] = t;
	}
	T& operator[](int i) {
		CHECK_BOUNDS(i, count);
		return ptr[i];
	}
	const T& operator[](int i) const {
		CHECK_BOUNDS(i, count);
		return ptr[i];
	}
	size_t bytes() { return count*sizeof(T); }
};

template<typename T>
void deepCopy(Array<T>* dst, const Array<T>* src) {
	dst->alloc(src->count);
	mempcy(dst->ptr, src->ptr, src->bytes());
}

template<typename T>
struct Stack {
	T* ptr;
	int count;
	int max_count;
	Stack() {
		ptr = 0;
		count = 0;
		max_count = 0;
	}
	void alloc(int _max_count) {
		ptr = (T*)malloc(_max_count*sizeof(T));
		assertm(ptr, "Out of memory! %p", ptr);
		count = 0;
		max_count = _max_count;
	}
	void free() {
		memFree(ptr);
		ptr = 0;
		count = 0;
		max_count = 0;
	}
	void push(const T& d) {
		assertm(count < max_count, "Stack overflow");
		ptr[count++] = d;
	}
	T pop() {
		assertm(count > 0, "Stack underflow");
		return ptr[--count];
	}
	void clear() {
		count = 0;
	}
	T& operator[](int i) {
		CHECK_BOUNDS(i, count);
		return ptr[i];
	}
	const T& operator[](int i) const {
		CHECK_BOUNDS(i, count);
		return ptr[i];
	}
	size_t bytes() { return count*sizeof(T); }
};

template<typename T>
struct Array2D {
	T* ptr;
	int count;
	int width,height;
	Array2D() {
		ptr = 0;
		count = 0;
		width = 0;
		height = 0;
	}
	void alloc(int _width, int _height) {
		width = _width;
		height = _height;
		count = width * height;
		ptr = (T*)malloc(count*sizeof(T));
		assertm(ptr, "Out of memory! %p", ptr);
	}
	void free() {
		memFree(ptr);
		ptr = 0;
		count = 0;
		width = 0;
		height = 0;
	}
	int idx(int x, int y) {
		return y*width + x;
	}
	T& operator()(int x, int y) {
		return ptr[y*width + x];
	}
	const T& operator()(int x, int y) const {
		return ptr[y*width + x];
	}
	T& operator[](int i) {
		CHECK_BOUNDS(i, count);
		return ptr[i];
	}
	const T& operator[](int i) const {
		CHECK_BOUNDS(i, count);
		return ptr[i];
	}
	void fill(const T& t) {
		for (int i = 0; i < count; ++i)
			ptr[i] = t;
	}
	size_t bytes() const { return count*sizeof(T); }
};
template<typename T>
void deepCopy(Array2D<T>* dst, const Array2D<T>* src) {
	if ((dst->width != src->width) || (dst->height != src->height)) {
		dst->free();
		dst->alloc(src->width, src->height);
	}
	memcpy(dst->ptr, src->ptr, src->bytes());
}

template<typename T>
struct Array3D {
	T* ptr;
	int count;
	int width,height,depth;
	int width_x_depth; //to accelerate operator()
	Array3D() {
		ptr = 0;
		count = 0;
		width = 0;
		height = 0;
		depth = 0;
		width_x_depth = 0;
	}
	void alloc(int _width, int _height, int _depth) {
		width = _width;
		height = _height;
		depth = _depth;
		width_x_depth = width*depth;
		count = width * height * depth;
		ptr = (T*)malloc(count*sizeof(T));
		assertm(ptr, "Out of memory! %p", ptr);
	}
	void free() {
		memFree(ptr);
		ptr = 0;
		count = 0;
		width = 0;
		height = 0;
		depth = 0;
		width_x_depth = 0;
	}
	T& operator()(int x, int y, int z) { return ptr[y*(width_x_depth) + x*(depth) + z]; }
	T& operator[](int i) { return ptr[i]; }
	void fill(const T& t) {
		for (int i = 0; i < count; ++i)
			ptr[i] = t;
	}
	size_t bytes() { return count*sizeof(T); }
};

template<typename T>
struct DoubleBuffer {
	T buffs[2];
	T* front = &buffs[0];
	T* back = &buffs[1];

	T& operator[](int i) { return buffs[i]; }
	void swap() {
		T* temp = front;
		front = back;
		back = temp;
	}
};

template<int AVG_PERIOD, int ROLL_WINDOW>
struct RollingAvg {
	static const int roll_window = ROLL_WINDOW;
	float avg;
	int avg_idx;
	float roll[ROLL_WINDOW];
	int roll_idx;
	inline RollingAvg() {
		reset();
	}
	inline void reset() {
		avg = 0.f;
		avg_idx = 0;
		memset(roll, 0, sizeof(roll));
		roll_idx = 0;
	}
	inline void add(float v) {
		avg += v;
		if (++avg_idx == AVG_PERIOD) {
			roll[roll_idx] = avg / avg_idx;
			roll_idx = (roll_idx + 1) % ROLL_WINDOW;
			avg_idx = 0;
			avg = 0.f;
		}
	}
};

/*
template<typename T, int MAX_COUNT>
struct StaticStack {
	T ptr[MAX_COUNT];
	static const int max_count = MAX_COUNT;
	int count;
	StaticStack() {
		count = 0;
	}
	void push(const T& d) {
		assertm(count < max_count, "Static Stack overflow");
		ptr[count++] = d;
	}
	void pop() {
		assertm(count > 0, "Static Stack underflow");
		--count;
	}
	void clear() {
		count = 0;
	}
	T& operator[](int i) { return ptr[i]; }
	const T& operator[](int i) const { return ptr[i]; }
	size_t bytes() { return count*sizeof(T); }
};
*/

#define fori(container) \
  for (int64_t i = 0; i < container.count; ++i)

#define foreach(container) \
	for (int64_t)

#endif
