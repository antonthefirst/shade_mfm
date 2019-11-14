#pragma once
#include "hashed_string.h"
#include "container.h"

enum TimestampType {
	TIMESTAMP_MARKER,
	TIMESTAMP_START,
	TIMESTAMP_STOP,
};

struct TimestampLogEntry {
	HashedString label;
	TimestampType type;
};

struct Range {
	HashedString label;
	int depth;
	s64 t_start;
	s64 t_stop;
	int which;
};

struct RangeSum {
	HashedString label;
	int depth;
	s64 t_sum;
	int t_sum_count;
};

struct TimestampReport {
	Bunch<Range> ranges;
	Bunch<RangeSum> range_sums;
};

struct TimestampLog {
	Bunch<TimestampLogEntry> entries;

	void clear();
	void marker(HashedString hstr);
	void start(HashedString hstr);
	void stop();

	void report(s64 t_frame_start, s64 t_frame_start_next, const s64* timestamps, int timestamps_count, TimestampReport* report);
};

void timestampReportGui(const char* id, const TimestampReport* report, bool* show_timeline, float ms_per_frame, double millisec_per_count);

struct CpuTimestampLog {
	TimestampLog log;
	TimestampReport report;
	bool show_timeline = false;
	Bunch<s64> timestamps;
	s64 t_frame_start = 0;

	void newFrame();
	void marker(HashedString hstr);
	void start(HashedString hstr);
	void stop();
};

#include "wrap/evk.h"
#define QUERIES_PER_FRAME_MAX (4096)

struct GpuTimestampLog {
	struct Frame {
		TimestampLog log;
		VkQueryPool  query_pool;
		int          query_count;
		Bunch<s64>   timestamps;
	};
	Bunch<Frame> frames;
	
	int w = 0; // write (request) index
	int q = 0; // query index
	int r = 0; // report index
	TimestampReport report;
	bool show_timeline = false;
	bool stale_report;
	VkCommandBuffer cb;

	void destroy();

	void newFrame(VkCommandBuffer cb);
	void marker(HashedString hstr);
	void start(HashedString hstr);
	void stop();
};