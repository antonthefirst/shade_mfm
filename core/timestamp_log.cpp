#include "timestamp_log.h"
#include "cpu_timer.h"
#include "imgui/imgui.h"


//-----------------------------------------//
//   Hash table for timestamp ranges       //
//-----------------------------------------//

struct TimerHashEntry {
	HashedString hs;
	TimerHashEntry* next;
	int depth;
	s64 t_sum;
	int t_sum_count;
};

#define TIMER_COUNT_MAX 256
#define TIMER_HASH_TABLE_CAPACITY (TIMER_COUNT_MAX*2)
struct TimerHash {
	TimerHashEntry table[TIMER_HASH_TABLE_CAPACITY];
	TimerHashEntry block[TIMER_COUNT_MAX];
	int block_next_idx = 0;
	TimerHash() {
		clear();
	}
	void clear() {
		memset(this, 0, sizeof(TimerHash));
	}
	TimerHashEntry* getInternal(HashedString hs, TimerHashEntry** last_found) {
		int i = hs.hash % TIMER_HASH_TABLE_CAPACITY;
		TimerHashEntry* e = &table[i];
		while (e) {
			if (e->hs.str == 0) { // blank entry
				*last_found = e;
				return 0;
			}
			else if (e->hs.hash == hs.hash) { // hash matches
				*last_found = e;
				return e; // for now, just assume the strings must match!
			}
			else if (e->next) { // check more entries
				e = e->next;
			}
			else { // make a new entry
				*last_found = e;
				return 0;
			}
		}
		assert(false);
		return 0;
	}
	TimerHashEntry* get(HashedString hs) {
		TimerHashEntry* last_found;
		return getInternal(hs, &last_found);
	}
	TimerHashEntry* getOrAdd(HashedString hs, bool* added) {
		TimerHashEntry* last_found;
		if (TimerHashEntry* found = getInternal(hs, &last_found)) {
			*added = false;
			return found;
		}
		else {
			*added = true;
			if (last_found->hs.str == 0) { // blank entry
				last_found->hs = hs;
				return last_found;
			}
			else {
				assert(last_found->next == 0);
				if (block_next_idx < TIMER_COUNT_MAX) { // make a new entry
					last_found->next = &block[block_next_idx++];
					last_found->next->hs = hs;
					return last_found->next;
				}
				else {
					log("[Timer] Block allocator full!\n");
					assert(false);
					*added = false;
					return 0;
				}
			}
		}
	}
	void dev_stats() {
		int table_entries = 0;
		int chain_longest = 0;
		int chain_sum = 0;
		int chain_count = 0;
		for (unsigned i = 0; i < ARRSIZE(table); ++i) {
			if (table[i].hs.str != 0) {
				table_entries += 1;
				int chain = 0;
				TimerHashEntry* e = table[i].next;
				if (e)
					chain_count += 1;
				while (e) {
					chain += 1;
					e = e->next;
				}
				chain_longest = max(chain_longest, chain);
				chain_sum += chain;
			}
		}
		log("%d table entries (%3.2f), chained %d/%d (%3.2f), %d block entries (%3.2f), longest chain %d, avg chain %3.2f, chain count %d\n",
			table_entries, table_entries / float(ARRSIZE(table)) * 100.0f,
			chain_count, table_entries + block_next_idx, float(chain_count) / (table_entries + block_next_idx) * 100.0f,
			block_next_idx, block_next_idx / float(ARRSIZE(block)),
			chain_longest,
			chain_count > 0 ? chain_sum / float(chain_count) : 0.0f,
			chain_count);
	}
};

//-----------------------------------------//
//           Timestamp Log Core            //
//-----------------------------------------//

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
	ranges.clear();
	Bunch<RangeSum>& range_sums = report->range_sums;
	range_sums.clear();
	static Bunch<int> range_stack;
	range_stack.clear_and_reserve(TIMER_COUNT_MAX);
	static TimerHash  sum_table;
	sum_table.clear();
	static Bunch<HashedString> hash_stack;
	hash_stack.clear();
	static Bunch<HashedString> order;
	order.clear_and_reserve(TIMER_COUNT_MAX);

	// add the total frame
	Range r;
	r.label = HS("Frame");
	r.depth = 0;
	r.t_start = t_frame_start;
	r.t_stop = t_frame_start_next;
	ranges.push(r);
	range_stack.push(0);
	hash_stack.push(r.label);
	bool added;
	TimerHashEntry* s = sum_table.getOrAdd(r.label, &added);
	assert(s && added);
	order.push(r.label);
	s->depth = 0;
	s->t_sum = r.t_stop - r.t_start;
	s->t_sum_count = 1;
	r.which = 0;

	for (int i = 0; i < entries.count; ++i) {
		const TimestampLogEntry& e = entries[i];
		if (e.type == TIMESTAMP_MARKER) {
			// #TODO
		} else if (e.type == TIMESTAMP_START) {
			Range& r = ranges.push();
			r.depth = range_stack.count;
			r.t_start = timestamps[i];
			range_stack.push(ranges.count-1);
			HashedString hs = e.label;
			hs.hash = WangHash(hash_stack.top().hash ^ hs.hash);
			hash_stack.push(hs);
			r.label = hs;
			bool added;
			if (TimerHashEntry* s = sum_table.getOrAdd(hs, &added)) {
				if (added) {
					order.push(hs);
					s->depth = r.depth;
				}
				r.which = s->t_sum_count;
			}
		} else if (e.type == TIMESTAMP_STOP) {
			assert(range_stack.count > 0);
			Range& r = ranges[range_stack.top()];
			r.t_stop = timestamps[i];
			range_stack.pop();
			if (TimerHashEntry* s = sum_table.get(hash_stack.top())) {
				s->t_sum += r.t_stop - r.t_start;
				s->t_sum_count += 1;
				hash_stack.pop();
			}
			else {
				assert(false);
			}
		} else {
			assert(false);
		}
	}

	for (int i = 0; i < order.count; ++i) {
		TimerHashEntry* s = sum_table.get(order[i]);
		assert(s);
		RangeSum& r = range_sums.push();
		r.label = s->hs;
		r.depth = s->depth;
		r.t_sum = s->t_sum;
		r.t_sum_count = s->t_sum_count;
	}
}

//-----------------------------------------//
//           GUI Visualization             //
//-----------------------------------------//
static u32 col_from_hash(u32 hash) {
#if 1
	hash = WangHash(hash);
	float h = (hash & 0xffff) / float(0xffff);
	float v = lerp(0.4f,0.7f, ((hash>>16)&0xff) / float(0xff));
	float s = lerp(0.4f,0.7f, ((hash>>24)&0xff) / float(0xff));
	vec4 col;
	col.w = 1.0f;
	gui::ColorConvertHSVtoRGB(h,s,v, col.x, col.y, col.z);
	return gui::ColorConvertFloat4ToU32(col);
#else
	return hash | 0xff000000;
#endif
}
void timestampReportGui(const char* id, const TimestampReport* report, float ms_per_frame, double msec_per_count) {
	const Bunch<Range>& ranges = report->ranges;
	const Bunch<RangeSum>& range_sums = report->range_sums;
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
		
	ImU32 label_cols[6] = { 0xff404080, 0xff408040, 0xff804040, 0xff408080, 0xff804080, 0xff808040};
	int label_chars_max = 0;
	for (int i = 0; i < range_sums.count; ++i)
		label_chars_max = max(label_chars_max, (int)strlen(range_sums[i].label.str));
	const int label_count = range_sums.count;
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
		HashedString hovered_label = { };
		const vec2 cp_line = gui::GetCursorScreenPos();
		for (int i = 0; i < range_sums.count; ++i) {
			float ms = range_sums[i].t_sum / float(range_sums[i].t_sum_count) * msec_per_count;

			const vec2 cp = gui::GetCursorScreenPos();
			
			dl->AddRectFilled(cp, cp + vec2(bars_x_start - gui::GetFont()->FallbackAdvanceX, gui::GetTextLineHeightWithSpacing()), col_from_hash(range_sums[i].label.hash));
			gui::Text("%*s  %6.3f", -label_chars_max, range_sums[i].label.str, ms);
			if (gui::GetMousePos().x >= cp.x && gui::GetMousePos().x < (cp.x + bars_x_start) &&
				gui::GetMousePos().y >= cp.y && gui::GetMousePos().y < (cp.y + gui::GetTextLineHeightWithSpacing())) {
				hovered_label = range_sums[i].label;
			}
		}
		if (show_timeline) {
			for (int i = 0; i < ranges.count; ++i) {
				float ms = (ranges[i].t_stop - ranges[i].t_start) * msec_per_count;
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
				vec2 e = vec2(bars_x_start + double(ranges[i].t_stop - time_of_frame_start) * msec_per_count * pixels_from_ms, gui::GetTextLineHeightWithSpacing());
				dl->AddRectFilled(cp + s, cp + e, col_from_hash(ranges[i].label.hash));
				if (HashedStringEq(ranges[i].label, hovered_label))
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

//-----------------------------------------//
//   CPU/GPU specific implementations      //
//-----------------------------------------//

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
	timestampReportGui("CPU TIMERS", &ctimer.report, 1000.0f / evk.win.RefreshRate, 1000.0 / double(time_frequency())); // ticks per second -> milliseconds per tick
}


void GpuTimestampLog::destroy() {
	for (int i = 0; i < frames.count; ++i)
		vkDestroyQueryPool(evk.dev, frames[i].query_pool, evk.alloc);
	frames.clear();
}

static void insertFrame(Bunch<GpuTimestampLog::Frame>& frames, int i) {
	GpuTimestampLog::Frame f;
	VkResult err;
	VkQueryPoolCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
	info.queryType = VK_QUERY_TYPE_TIMESTAMP;
	info.queryCount = QUERIES_PER_FRAME_MAX;
	err = vkCreateQueryPool(evk.dev, &info, evk.alloc, &f.query_pool);
	evkCheckError(err);
	//vkCmdResetQueryPool(cb, f.query_pool, 0, QUERIES_PER_FRAME_MAX);
	//log("Init reset %d\n", i);
	f.query_count = 0;
	frames.insert(i, f);
}
static void pushFrame(Bunch<GpuTimestampLog::Frame>& frames) {
	GpuTimestampLog::Frame& f = frames.push();
	VkResult err;
	VkQueryPoolCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
	info.queryType = VK_QUERY_TYPE_TIMESTAMP;
	info.queryCount = QUERIES_PER_FRAME_MAX;
	err = vkCreateQueryPool(evk.dev, &info, evk.alloc, &f.query_pool);
	evkCheckError(err);
	//vkCmdResetQueryPool(cb, f.query_pool, 0, QUERIES_PER_FRAME_MAX);
	//log("Init reset %d\n", i);
	f.query_count = 0;
}
void GpuTimestampLog::newFrame(VkCommandBuffer frame_cb) {
	cb = frame_cb;
	stale_report = false;

	//#TODO this shouldn't matter, it can resize dynamically
	if (frames.count == 0) {
		destroy();

		int frame_count = 3; // 2 frame swapchain + 1

		for (int i = 0; i < frame_count; ++i)
			pushFrame(frames);
			//insertFrame(frames, i);
		w = -1;
		q = r = 0;
	}

	/* Query available data */
	{
		if (frames[q].query_count > 0) {
			if (frames[q].timestamps.count == 0) {
				frames[q].timestamps.setgarbage(frames[q].query_count);
				VkResult res = vkGetQueryPoolResults(evk.dev, frames[q].query_pool, 0, frames[q].query_count, frames[q].timestamps.bytes(), frames[q].timestamps.ptr, sizeof(int64_t), VK_QUERY_RESULT_64_BIT);
				if (res == VK_SUCCESS) {
					//vkCmdResetQueryPool(cb, frames[q].query_pool, 0, frames[q].query_count);
					//log("Reset pool %d (0x%x of %d queries)\n", q, frames[q].query_pool, frames[q].query_count);
					frames[q].query_count = 0;
					q = (q + 1) % frames.count;
					//log("Query: success\n");
				}
				else {
					log("Query: not ready\n");
					frames[q].timestamps.clear(); // undo the resize that was done before query
				}
			}
			else {
				log("Query: timestamps not shown yet\n");
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
			//log("Composing from r=%d n=%d timestamps, log has %d entries\n", frames[r].timestamps.count, frames[n].timestamps.count, frames[r].log.entries.count);
			frames[r].log.report(frames[r].timestamps[0], frames[n].timestamps[0], frames[r].timestamps.ptr + 1, frames[r].timestamps.count - 1, &report);
			frames[r].log.clear();
			frames[r].timestamps.clear();
			r = n;
			//log("Compose: success\n");
		}
		else {
			log("Compose: not enough data (r=%d n=%d)\n", frames[r].timestamps.count, frames[n].timestamps.count);
			stale_report = true;
		}
	}

	/* Advance write head */
	{
		int n = (w + 1) % frames.count;

		// if we've run into the query head and it hasn't gotten the results yet, then we need to add a buffer frame
		if (n == q && frames[n].query_count > 0) {
			log("Write head ran into Query head, adding a frame\n");
			assert(false);
		}
		if (n == r && frames[n].log.entries.count > 0) {
			log("Write head ran into Read head, adding a frame\n");
			//insertFrame(frames, n);
			//r = (r + 1) % frames.count;
			//q = (q + 1) % frames.count;
			//log("space at %d: %d %d\n", n, frames[n].log.entries.count, frames[n].query_count);
			assert(false);
		}
		w = n;

		// write the "start of frame" query
		//log("Frame use pool %d 0x%x\n", w, frames[w].query_pool);
		vkCmdResetQueryPool(cb, frames[w].query_pool, 0, QUERIES_PER_FRAME_MAX);
		vkCmdWriteTimestamp(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, frames[w].query_pool, frames[w].query_count++);
	}
}
void GpuTimestampLog::marker(HashedString hstr) {
	frames[w].log.marker(hstr);
	vkCmdWriteTimestamp(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, frames[w].query_pool, frames[w].query_count++);
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
	//gui::Begin("GPU TIMERS");
	//gui::Text("Stale: %d", gpu_timer.stale_report);
	//gui::Text("Frames: %d", gpu_timer.frames.count);
	//gui::End();
	timestampReportGui("GPU TIMERS", &gpu_timer.report, 1000.0f / evk.win.RefreshRate, evk.phys_props.limits.timestampPeriod / 1000000.0);
}

