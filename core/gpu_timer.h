#pragma once

struct GPUCounterFrame;

struct GPUTimer {
	int frame_count = 0;
	GPUCounterFrame* frames = 0;
	int r = 0;
	int w = 0;

	// read latest results
	GPUCounterFrame* read();
	// reset frame (will allocate needed data on first call)
	void reset();

	//start and stop timers
	void start(const char* label);
	void stop();

	// de-allocate
	void destroy();
}; 

void guiGPUTimer(GPUCounterFrame* timer, const char* name = 0);
float getTimeOfLabel(GPUCounterFrame* frame, const char* name);