#include "cpu_timer.h"

static int64_t time_start = 0;

void timeSetStart() {
	time_start = time_counter();
}

double secSinceStart() {
	return time_to_sec(time_counter() - time_start);
}
int64_t timeCounterSinceStart() {
	return time_counter() - time_start;
}