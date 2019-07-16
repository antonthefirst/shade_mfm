#pragma once

#include "core/maths.h"
#include "core/log.h" // for log

#ifdef _WIN32
#include <Windows.h> // for QueryPerformanceCounter and QueryPerformanceFrequency
static __inline int64_t time_counter() {
	LARGE_INTEGER ticks;
	QueryPerformanceCounter(&ticks);
	return (int64_t)ticks.QuadPart;
}
static __inline int64_t time_frequency() {
	LARGE_INTEGER ticksPerSecond;
	QueryPerformanceFrequency(&ticksPerSecond);
	return (int64_t)ticksPerSecond.QuadPart;
}

#else
#include <time.h>     /* For clock_gettime, CLOCK_MONOTONIC */
#include <string.h>   /* For memcpy */
static __inline int64_t time_counter() {
     struct timespec ts1;
     if (clock_gettime(CLOCK_MONOTONIC, &ts1) != 0) {
         assert("Can't clock :( where's my clock?");
         return -1L;
     }
	 return ts1.tv_nsec;
}
static __inline int64_t time_frequency() {
	return 1000000000L;
}
#endif

static __inline double time_to_sec(int64_t t) {
	static const double f = (double)time_frequency();
	return double(t) / f;
}
static __inline double time_to_msec(int64_t t) {
	return time_to_sec(t) * 1000.0;
}

// set the global time starting point for the seconds functions below
void timeSetStart();

// seconds since timeSetStart has been called (at start of application). avoid using this for anything other than prototypes, since even double time gets inaccurate far from zero
double secSinceStart();

// ticks since timeSetStart has been called (at start of application).
int64_t timeCounterSinceStart();

// simple timer for code blocks
struct BlockTimer {
	int64_t t;
	const char* name;
	BlockTimer(const char* n = "") {
		name = n;
		t = time_counter();
	}
	~BlockTimer() {
		t = time_counter() - t;
		log("Block time of %s: %fms (%lld ticks)\n", name, time_to_msec(t), t);
	}
};


#define TIMER_TREE_DEBUG
//#define DISABLE_TIMER

#ifdef TIMER_TREE_DEBUG
#include <stdio.h> //for printf
#endif

#define TIMER_TREE_MAX_TIMERS 256

struct TimerResult {
	const char* name;   //name of the timer
	const char* parent; //name of the parent
	int depth;          //the depth of the tree, useful for drawing
	float time;         //time in milliseconds
	int count;          //number of times the timer was started/stopped paused/unpaused
};

struct TimerState {
	const char* name;
	uint64_t start;
	uint64_t total;
	bool paused;
	int count;
	TimerResult* res;
};

class TimerTree {
	TimerResult results[TIMER_TREE_MAX_TIMERS];
	TimerResult* resultPtr;
	TimerState stateStack[TIMER_TREE_MAX_TIMERS];
	TimerState* statePtr;
public:
	TimerTree() {
		reset();
	}
	inline void reset() {
#ifndef DISABLE_TIMER
		statePtr = stateStack-1;
		resultPtr = results;
#endif
	}
	inline void start(const char* name) {
#ifndef DISABLE_TIMER
#ifdef TIMER_TREE_DEBUG
		if ((statePtr-stateStack) >= TIMER_TREE_MAX_TIMERS-1) { log("TimerTree (start): Can't start any more timers, stack full.\n"); return; }
		if ((resultPtr-results) >= TIMER_TREE_MAX_TIMERS-2) { log("TimerTree (start): Can't start any more timers, results full (%ld).\n", (resultPtr-results)); return; } //-2 because resultPtr starts equal to results, unlike statePtr
#endif
		++statePtr;
		statePtr->name = name;
		statePtr->start = time_counter();
		statePtr->total = 0;
		statePtr->count = 0;
		statePtr->paused = false;
		statePtr->res = resultPtr++;
#endif
	}
	//same as start, but initializes start to 0 (to be used with "unpause/pause" subtiming)
	inline void startPaused(const char* name) {
#ifndef DISABLE_TIMER
#ifdef TIMER_TREE_DEBUG
		if ((statePtr-stateStack) >= TIMER_TREE_MAX_TIMERS-1) { log("TimerTree (startPaused): Can't start any more timers, stack full.\n"); return; }
		if ((resultPtr-results) >= TIMER_TREE_MAX_TIMERS-2) { log("TimerTree (start): Can't start any more timers, results full (%ld).\n", (resultPtr-results)); return; } //-2 because resultPtr starts equal to results, unlike statePtr
#endif
		++statePtr;
		statePtr->name = name;
		statePtr->start = 0;
		statePtr->total = 0;
		statePtr->count = 0;
		statePtr->paused = true;
		statePtr->res = resultPtr++;
#endif
	}
	inline void unpause() {
#ifndef DISABLE_TIMER
		statePtr->start = time_counter();
		statePtr->paused = false;
#endif
	}
	inline void pause() {
#ifndef DISABLE_TIMER
		uint64_t currTime = time_counter();
		statePtr->total += currTime - statePtr->start;
		statePtr->paused = true;
		++statePtr->count;
#endif
	}
	inline void stop() {
#ifndef DISABLE_TIMER
		//get time asap
		uint64_t currTime = time_counter();

#ifdef TIMER_TREE_DEBUG
		if ((statePtr-stateStack) < 0) { printf("TimerTree (stop): No timers to stop.\n"); return; }
#endif
		//if the timer was running, add the current time to the total
		if (!statePtr->paused) {
			statePtr->total += currTime - statePtr->start;
			++statePtr->count;
		}
		//if it was not then do nothing (total was already computed by pause() or not at all)
		else {

		}

		static float tfreq = 0.0f;
		if (tfreq==0.0f) tfreq = (float)time_frequency();
		statePtr->res->time = statePtr->total/tfreq*1000.0f;
		statePtr->res->name = statePtr->name;
		statePtr->res->parent = (statePtr-1)->name;
		statePtr->res->depth = int(statePtr - stateStack);
		statePtr->res->count = statePtr->count;
		--statePtr;
#endif
	}
	inline int getResults(TimerResult* res, int maxTimers) {
#ifndef DISABLE_TIMER
		int num = min(maxTimers, int(resultPtr-results));
		memcpy(res, results, num*sizeof(TimerResult));
		return num;
#else
		return 0;
#endif
	}
};
