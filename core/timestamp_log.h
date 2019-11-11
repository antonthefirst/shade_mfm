#pragma once
#include "hashed_string.h"
#include "container.h"

enum TimestampType {
	TIMESTAMP_MARKER,
	TIMESTAMP_BEGIN,
	TIMESTAMP_END,
};

struct TimestampLogEntry {
	HashedString label;
	TimestampType type;
};

struct TimestampLog {
	Bunch<TimestampLogEntry> entries;

	void clear();
	void marker(HashedString hstr);
	void begin(HashedString hstr);
	void end();

	void showGui(s64* timestamps, int timestamps_count, double nanosec_per_count);
};

struct CpuTimestampLog {
	TimestampLog log;
	Bunch<s64> timestamps;

	void newFrame();
	void marker(HashedString hstr);
	void begin(HashedString hstr);
	void end();
	void showGui();
};

#include <vulkan/vulkan.h>

struct GpuTimestampLog {
	struct Frame {
		TimestampLog log;
		VkQueryPool  query_pool;
		int          query_count;
	};
	Bunch<Frame> frames;

	void resize(int swapchain_frame_count);
	void destroy();

	void newFrame();
	void showGui();
};