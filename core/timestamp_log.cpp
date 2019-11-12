#include "timestamp_log.h"
#include "cpu_timer.h"
#include "imgui/imgui.h"


void TimestampLog::clear() {
	entries.clear();
}
void TimestampLog::marker(HashedString hstr) {
	TimestampLogEntry& e = entries.push();
	e.label = hstr;
	e.type  = TIMESTAMP_MARKER;
}
void TimestampLog::start(HashedString hstr) {
	TimestampLogEntry& e = entries.push();
	e.label = hstr;
	e.type  = TIMESTAMP_START;
}
void TimestampLog::stop() {
	TimestampLogEntry& e = entries.push();
	e.label = { };
	e.type  = TIMESTAMP_STOP;
}
void TimestampLog::report(s64 t_frame_start, s64 t_frame_start_next, const s64* timestamps, int timestamps_count, TimestampReport* report) {
	if (entries.count != timestamps_count) {
		assert(false);
		return;
	}

	/* Fill in the results */
	Bunch<Range>& ranges = report->ranges;
	static Bunch<int>   range_stack;
	ranges.clear();
	range_stack.clear();

	// add the total frame
	Range r;
	r.label = HS("Frame");
	r.depth = 0;
	r.t_start = t_frame_start;
	r.t_stop = t_frame_start_next;
	ranges.push(r);
	range_stack.push(0);

	for (int i = 0; i < entries.count; ++i) {
		const TimestampLogEntry& e = entries[i];
		if (e.type == TIMESTAMP_MARKER) {
			// #TODO
		} else if (e.type == TIMESTAMP_START) {
			Range& r = ranges.push();
			r.label = e.label;
			r.depth = range_stack.count;
			r.t_start = timestamps[i];
			range_stack.push(ranges.count-1);
		} else if (e.type == TIMESTAMP_STOP) {
			assert(range_stack.count > 0);
			Range& r = ranges[range_stack.top()];
			r.t_stop = timestamps[i];
			range_stack.pop();
		} else {
			assert(false);
		}
	}
}

void timestampReportGui(const char* id, const TimestampReport* report, double msec_per_count) {
	const Bunch<Range>& ranges = report->ranges;
	if (ranges.count == 0) return;
	/*
	if (gui::Begin(id)) {
		for (int i = 0; i < ranges.count; ++i) {
			const Range& r = ranges[i];
			float ms = double(r.t_stop - r.t_start) * msec_per_count;
			if (r.depth) gui::Indent(gui::GetStyle().IndentSpacing*r.depth);
			gui::Text("%*s  %6.3f", -(16 - 3 * r.depth), r.label.str, ms);
			if (r.depth) gui::Unindent(gui::GetStyle().IndentSpacing*r.depth);
		}
	} gui::End();
	*/
	const float ms_per_frame = 1000.0f / 144.0f;
	float frame_ms = (ranges[0].t_stop - ranges[0].t_start) * msec_per_count;
	s64 time_of_frame_start = ranges[0].t_start;

	static bool show_timeline = true;

	// apply hysteresis to frame_ms:
	// ignore frames that are insanely long or that aren't that much smaller than before
	static int frame_range_hyst = 0;
	int frame_range = int(frame_ms / ms_per_frame) + 1;
	if (frame_range < 16 && frame_range > frame_range_hyst)
		frame_range_hyst = frame_range;
	else if (frame_range < (frame_range_hyst - 1))
		frame_range_hyst = frame_range;
		
	const double nanosec_per_count = evk.phys_props.limits.timestampPeriod;
	ImU32 label_cols[6] = { 0xff404080, 0xff408040, 0xff804040, 0xff408080, 0xff804080, 0xff808040};
	int label_chars_max = 0;
	for (int i = 0; i < ranges.count; ++i)
		label_chars_max = max(label_chars_max, (int)strlen(ranges[i].label.str));
	const int label_count = ranges.count;
	const float pixels_from_ms = 15.0f; 
	const float bars_x_start = float(label_chars_max + 9) * gui::GetFont()->FallbackAdvanceX;

	gui::SetNextWindowSize(vec2(show_timeline ? 800.0f : bars_x_start + gui::GetStyle().WindowPadding.x, 0.0f));
	if (gui::Begin(id, 0, ImGuiWindowFlags_AlwaysAutoResize)) {
		if (show_timeline && gui::Button("collapse", vec2(bars_x_start - gui::GetFont()->FallbackAdvanceX, 0.f))) show_timeline = false;
		else if (!show_timeline && gui::Button("expand", vec2(bars_x_start - gui::GetFont()->FallbackAdvanceX, 0.f))) show_timeline = true;

		ImDrawList* dl = gui::GetWindowDrawList();
		const vec2 cs = gui::GetContentRegionAvail();

		const ImU32 major_grid_col = 0xff888888;
		const ImU32 minor_grid_col = 0xff444444;
		const ImU32 frame_grid_col = 0xff444488;
		const ImU32 bar_col = 0x80eeeeee;
		// Draw the grid:
		if (show_timeline) {
			const vec2 cp = gui::GetCursorScreenPos();
			// starting line:
			dl->AddLine(cp + vec2(bars_x_start, 0.0f), cp + vec2(bars_x_start, (label_count-1) * gui::GetTextLineHeightWithSpacing() + gui::GetTextLineHeight()), major_grid_col);
			float ms = 1.0f;

			while (ms < (frame_range_hyst * ms_per_frame)) {
				float x = bars_x_start + ms * pixels_from_ms;
				dl->AddLine(cp + vec2(x, 0.0f), cp + vec2(x, (label_count-1) * gui::GetTextLineHeightWithSpacing() + gui::GetTextLineHeight()), minor_grid_col);
				ms += 1.0f;
			}
			float x = bars_x_start + ms_per_frame * pixels_from_ms;
			dl->AddLine(cp + vec2(x, 0.0f), cp + vec2(x, (label_count-1) * gui::GetTextLineHeightWithSpacing() + gui::GetTextLineHeight()), frame_grid_col);
		}
		
		const vec2 cp_line = gui::GetCursorScreenPos();
		for (int i = 0; i < ranges.count; ++i) {
			float ms = (ranges[i].t_stop - ranges[i].t_start) * msec_per_count;

			const vec2 cp = gui::GetCursorScreenPos();
			//if (show_timeline)
				dl->AddRectFilled(cp, cp + vec2(bars_x_start - gui::GetFont()->FallbackAdvanceX, gui::GetTextLineHeightWithSpacing()), label_cols[i%ARRSIZE(label_cols)]);
			gui::Text("%*s  %6.3f", -label_chars_max, ranges[i].label.str, ms);
			bool hover_label = gui::GetMousePos().x >= cp.x && gui::GetMousePos().x < (cp.x + bars_x_start) &&
					gui::GetMousePos().y >= cp.y && gui::GetMousePos().y < (cp.y + gui::GetTextLineHeightWithSpacing());
			if (show_timeline) {
				/* feels redundant if there is room to draw over label
				if (gui::GetMousePos().x >= (cp.x + bars_x_start) &&
					gui::GetMousePos().y >= cp.y && gui::GetMousePos().y < (cp.y + gui::GetTextLineHeightWithSpacing())) {
					gui::BeginTooltip();
					gui::Text("%s: %6.3f", texts[i%ARRSIZE(texts)], ms);
					gui::EndTooltip();
				}
				*/
				const vec2 cp = cp_line + vec2(0.0f, ranges[i].depth * gui::GetTextLineHeightWithSpacing());
				vec2 s = vec2(bars_x_start + double(ranges[i].t_start - time_of_frame_start) * msec_per_count * pixels_from_ms, 0.0f);
				vec2 e = vec2(bars_x_start + double(ranges[i].t_stop  - time_of_frame_start) * msec_per_count * pixels_from_ms, gui::GetTextLineHeightWithSpacing());
				dl->AddRectFilled(cp + s, cp + e, label_cols[i%ARRSIZE(label_cols)]);
				if (hover_label)
					dl->AddRect(cp + s, cp + e, 0xffffffff); // useful for making sure "missing" labels are actually just invisible slivers. but on all the time gives a false size impression (negligible == tiny)

				// nice to know the exact number, but without also seeing the label you can't tell what the number belongs to, so still have to look over. tooltip of "label: time" is better (though requires mouse)
				// actually really nice if we use color, because easy to remember "blue is update" etc.
				if (ms > 1.0) {
					TempStr digits = TempStr("%.1f", ms);
					dl->AddText(cp + vec2((s.x + e.x)*0.5f - gui::GetFont()->FallbackAdvanceX * digits.len * 0.5f, s.y), 0xffffffff, digits.str);
					//dl->AddText(cp + vec2(e.x, s.y), 0xffffffff, digits.str);
					//dl->AddText(cp + s - vec2((strlen(texts[i]) * gui::GetFont()->FallbackAdvanceX), 0.0f), 0xffffffff, texts[i%ARRSIZE(texts)]);
				}
				
			}
		}
	} gui::End();

}

void CpuTimestampLog::newFrame() {
	s64 t_frame_start_next = time_counter();
	log.report(t_frame_start, t_frame_start_next, timestamps.ptr, timestamps.count, &report);
	timestamps.clear();
	log.clear();
	t_frame_start = t_frame_start_next;
}
void CpuTimestampLog::marker(HashedString hstr) {
	log.marker(hstr);
	timestamps.push(time_counter());
}
void CpuTimestampLog::start(HashedString hstr) {
	log.start(hstr);
	timestamps.push(time_counter());
}
void CpuTimestampLog::stop() {
	timestamps.push(time_counter());
	log.stop();
}


static CpuTimestampLog ctimer;
void ctimer_reset() {
	ctimer.newFrame();
}
void chashtimer_start(HashedString hs) {
	ctimer.start(hs);
}
void chashtimer_stop() {
	ctimer.stop();
}
void ctimer_gui() {
	timestampReportGui("CPU TIMERS", &ctimer.report, 1000.0 / double(time_frequency())); // ticks per second -> milliseconds per tick
}





void GpuTimestampLog::resize(int swapchain_frame_count) {
	if (swapchain_frame_count == frames.count) return; // maybe not a good idea, if the swapchain is rebuilt (for whatever reason) we may still want to at least reset the timers?

	destroy();

	int frame_count = swapchain_frame_count * 3;

	VkResult err;
	for (int i = 0; i < frame_count; ++i) {
		Frame& f = frames.push();
		VkQueryPoolCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
		info.queryType = VK_QUERY_TYPE_TIMESTAMP;
		info.queryCount = QUERIES_PER_FRAME_MAX;
		err = vkCreateQueryPool(evk.dev, &info, evk.alloc, &f.query_pool);
		evkCheckError(err);
		f.query_count = 0;
	}
	w = -1;
	q = r = 0;
}
void GpuTimestampLog::destroy() {
	for (int i = 0; i < frames.count; ++i)
		vkDestroyQueryPool(evk.dev, frames[i].query_pool, evk.alloc);
	frames.clear();
}
void GpuTimestampLog::newFrame(VkCommandBuffer frame_cb) {
	cb = frame_cb;
	stale_report = false;

	//#TODO this shouldn't matter, it can resize dynamically
	if (frames.count == 0)
		resize(2);

	/* Query available data */
	{
		if (frames[q].query_count > 0) {
			frames[q].timestamps.setgarbage(frames[q].query_count);
			VkResult res = vkGetQueryPoolResults(evk.dev, frames[q].query_pool, 0, frames[q].query_count, frames[q].timestamps.bytes(), frames[q].timestamps.ptr, sizeof(int64_t), VK_QUERY_RESULT_64_BIT);
			if (res == VK_SUCCESS) {
				vkCmdResetQueryPool(cb, frames[q].query_pool, 0, QUERIES_PER_FRAME_MAX);
				q = (q + 1) % frames.count;
				//log("Query: success\n");
			}
			else {
				log("Query: not ready\n");
				frames[q].timestamps.clear(); // undo the resize that was done before query
			}
		}
		else {
			log("Query: nothing to read\n");
		}
	}

	/* Compose a report */
	{
		int n = (r + 1) % frames.count;
		if (frames[r].timestamps.count && frames[n].timestamps.count) {
			//log("Composing from %d timestamps, log has %d entries\n", frames[r].timestamps.count, frames[r].log.entries.count);
			frames[r].log.report(frames[r].timestamps[0], frames[n].timestamps[0], frames[r].timestamps.ptr + 1, frames[r].timestamps.count - 1, &report);
			frames[r].log.clear();
			frames[r].timestamps.clear();
			r = n;
			//log("Compose: success\n");
		}
		else {
			log("Compose: not enough data\n");
			stale_report = true;
		}
	}

	/* Advance write head */
	{
		int n = (w + 1) % frames.count;
		if (n == r) {
			log("OVERWRITING READ HEAD!\n");
			//#TODO add array resizing here
		}
		w = n;

		// if the pool has never been used yet, reset it (usually it's reset after being queried)
		if (frames[w].query_count == 0)
			vkCmdResetQueryPool(cb, frames[w].query_pool, 0, QUERIES_PER_FRAME_MAX);
		frames[w].query_count = 0; // a bit gross, seems better to do it with the other reset. this all smells.

		// write the "start of frame" query
		vkCmdWriteTimestamp(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, frames[w].query_pool, frames[w].query_count++);
	}
}
void GpuTimestampLog::marker(HashedString hstr) {

}
void GpuTimestampLog::start(HashedString hstr) {
	frames[w].log.start(hstr);
	vkCmdWriteTimestamp(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, frames[w].query_pool, frames[w].query_count++);
}
void GpuTimestampLog::stop() {
	vkCmdWriteTimestamp(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, frames[w].query_pool, frames[w].query_count++);
	frames[w].log.stop();
}

static GpuTimestampLog gpu_timer;
void gtimer_reset(VkCommandBuffer cb) {
	gpu_timer.newFrame(cb);
}
void ghashtimer_start(HashedString label) {
	gpu_timer.start(label);
}
void ghashtimer_stop() {
	gpu_timer.stop();
}
void gtimer_gui() {
	gui::Begin("GPU TIMERS");
	gui::Text("Stale: %d", gpu_timer.stale_report);
	gui::Text("Frames: %d", gpu_timer.frames.count);
	gui::End();
	timestampReportGui("GPU TIMERS", &gpu_timer.report, evk.phys_props.limits.timestampPeriod / 1000000.0);
}

