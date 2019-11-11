#include "timestamp_log.h"
#include "cpu_timer.h"
#include "imgui/imgui.h"

namespace {

struct Range {
	HashedString label;
	int depth;
	s64 t_begin;
	s64 t_end;
};

};

void TimestampLog::clear() {
	entries.clear();
}
void TimestampLog::marker(HashedString hstr) {
	TimestampLogEntry& e = entries.push();
	e.label = hstr;
	e.type  = TIMESTAMP_MARKER;
}
void TimestampLog::begin(HashedString hstr) {
	TimestampLogEntry& e = entries.push();
	e.label = hstr;
	e.type  = TIMESTAMP_BEGIN;
}
void TimestampLog::end() {
	TimestampLogEntry& e = entries.push();
	e.label = { };
	e.type  = TIMESTAMP_END;
}
void TimestampLog::showGui(s64* timestamps, int timestamps_count, double nanosec_per_count) {
	if (entries.count != timestamps_count) {
		assert(false);
		return;
	}

	/* Fill in the results */
	static Bunch<Range> range_results;
	static Bunch<int>   range_stack;
	range_results.clear();
	range_stack.clear();
	for (int i = 0; i < entries.count; ++i) {
		const TimestampLogEntry& e = entries[i];
		if (e.type == TIMESTAMP_MARKER) {
			// #TODO
		} else if (e.type == TIMESTAMP_BEGIN) {
			Range& r = range_results.push();
			r.label = e.label;
			r.depth = range_stack.count;
			r.t_begin = timestamps[i];
			range_stack.push(range_results.count-1);
		} else if (e.type == TIMESTAMP_END) {
			assert(range_stack.count > 0);
			Range& r = range_results[range_stack.top()];
			r.t_end = timestamps[i];
			range_stack.pop();
		} else {
			assert(false);
		}
	}

	/* Show the results */
	if (gui::Begin("TIMESTAMP LOG")) {
		for (int i = 0; i < range_results.count; ++i) {
			const Range& r = range_results[i];
			float ms = double(r.t_end - r.t_begin) / double(time_frequency()) * 1000.0;
			if (r.depth) gui::Indent(gui::GetStyle().IndentSpacing*r.depth);
			gui::Text("%*s  %6.3f", -(16 - 3 * r.depth), r.label.str, ms);
			if (r.depth) gui::Unindent(gui::GetStyle().IndentSpacing*r.depth);
		}
	} gui::End();
}

void CpuTimestampLog::newFrame() {
	timestamps.clear();
	log.clear();
}
void CpuTimestampLog::marker(HashedString hstr) {
	log.marker(hstr);
	timestamps.push(time_counter());
}
void CpuTimestampLog::begin(HashedString hstr) {
	log.begin(hstr);
	timestamps.push(time_counter());
}
void CpuTimestampLog::end() {
	timestamps.push(time_counter());
	log.end();
}
void CpuTimestampLog::showGui() {
	log.showGui(timestamps.ptr, timestamps.count, double(time_frequency()));
}

static CpuTimestampLog ctimer;
void ctimer_reset() {
	ctimer.newFrame();
}
void chashtimer_start(HashedString hs) {
	ctimer.begin(hs);
}
void chashtimer_stop() {
	ctimer.end();
}
void ctimer_gui() {
	ctimer.showGui();
}