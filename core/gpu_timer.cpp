#include "gpu_timer.h"
#include "stdlib.h" // for malloc
#include <string.h> // for memmove
#include "assert.h"
#include "core/basic_types.h"

#include "core/log.h" //for log

#include <GL/gl3w.h>
#include <GLFW/glfw3.h>

struct GPUCounter {
	GLuint id;
	const char* label;
};

struct GPUCounterFrame {
	int count;
	int max_count;
	GPUCounter* counters;
	bool blocked;
	void init();
	void destroy();
	void clear();
	bool isEmpty();
	bool isReady();
	void reserve(int num_counters);
	void querySubmit(const char* label);
	void queryGet(int counter, u64* time, const char** label);
};

void GPUCounterFrame::reserve(int num_counters_desired) {
	//int desired_count = int(float(num_counters_desired) / 16 + 1) * 16; // round up to nearest 16 counters
	int desired_count = num_counters_desired;
	if (desired_count > max_count) {
		counters = (GPUCounter*)realloc(counters, desired_count * sizeof(GPUCounter));
		assert(counters);
		for (int i = max_count; i < desired_count; ++i) {
			glGenQueries(1, &counters[i].id);
			counters[i].label = 0;
		}
		max_count = desired_count;
	}
}
void GPUCounterFrame::init() {
	count = 0;
	max_count = 0;
	counters = 0;
	reserve(1);
	blocked = true;
}
void GPUCounterFrame::destroy() {
	free(counters);
}
void GPUCounterFrame::clear() {
	count = 0;
	blocked = false;
}
bool GPUCounterFrame::isEmpty() {
	return count == 0;
}
bool GPUCounterFrame::isReady() {
	assert(count > 0);
	int res_avail = 0;
	glGetQueryObjectiv(counters[count-1].id, GL_QUERY_RESULT_AVAILABLE, &res_avail); // check the end query, since if that's ready all the previous ones are too
	return res_avail == GL_TRUE;
}
void GPUCounterFrame::querySubmit(const char* label) {
	if (blocked) return;
	if (count >= max_count)
		reserve(count + 1);
	assert(counters);
	GPUCounter& c = counters[count++];
	glQueryCounter(c.id, GL_TIMESTAMP);
	c.label = label;
}
void GPUCounterFrame::queryGet(int counter, u64* time, const char** label) {
	glGetQueryObjectui64v(counters[counter].id, GL_QUERY_RESULT, time);
	*label = counters[counter].label;
}
void GPUTimer::reset() {
	if (!frames) {
		frame_count = 2;
		frames = (GPUCounterFrame*)malloc(frame_count * sizeof(GPUCounterFrame));
		r = 0;
		w = frame_count - 1;
		for (int i = 0; i < frame_count; ++i)
			frames[i].init();
	}
	assert(frames);
	int n = (w + 1) % frame_count;
	if (!(frames[n].isEmpty() || (n != r && frames[n].isReady()))) { // if the next frame isn't empty, or isn't ready and not being read, then we need more frames
		frames = (GPUCounterFrame*)realloc(frames, (frame_count+1) * sizeof(GPUCounterFrame));
		assert(frames);
		frame_count = frame_count + 1;
		// shift blocking data forward by one (if any)
		int num_to_move = ((frame_count - 1) - (w + 1));
		if (num_to_move) memmove(frames + w + 1 + 1, frames + w + 1, num_to_move * sizeof(GPUCounterFrame));
		frames[w+1].init();
	}
	w = n;
	frames[w].clear();
}
void GPUTimer::start(const char* label) {
	if (!frames) return;
	frames[w].querySubmit(label);
}
void GPUTimer::stop() {
	if (!frames) return;
	frames[w].querySubmit(NULL);
}
GPUCounterFrame* GPUTimer::read() {
	if (!frames) return NULL;
	while (true) { // advance forward to the most recent frame
		int n = (r + 1) % frame_count;
		if (r != w && !frames[n].isEmpty() && frames[n].isReady())
			r = n;
		else
			break;
	}

	if (!frames[r].isEmpty() && frames[r].isReady()) 
		return &frames[r];
		return NULL;
}
void GPUTimer::destroy() {
	if (frames) {
		for (int i = 0; i < frame_count; ++i) {
			frames[i].destroy();
		}
		free(frames);
	}
}

#include "imgui/imgui.h"
#include "core/maths.h"
#include "core/vec3.h"
#include "core/crc.h"
#include <vector>
struct GPUTimerGUIEntry {
	const char* label;
	u64 start_time;
	u64 end_time;
	int depth;
	bool is_leaf;
};
static std::vector<GPUTimerGUIEntry> entries;
static std::vector<int> entry_stack;

static vec3 getColor(int i, const GPUTimerGUIEntry& e) {
	//u32 k = WangHash(i);
	u32 k = 0xdeadbeef;
	k = crc32(k, e.label, strlen(e.label));
	return normalize(vec3(WangHashFloat(k^0xdeadbeef, 0.f, 1.f), WangHashFloat(k^0x1337c0de, 0.f, 1.f), WangHashFloat(k^0xabcdef89, 0.f, 1.f)));
}
void guiGPUTimer(GPUCounterFrame* frame, const char* name) {	

	if (gui::Begin(name ? name : "GPU Timer")) {
		if (frame) {
			entries.clear();
			entry_stack.clear();
			for (int i = 0; i < frame->count; ++i) {
				u64 t;
				const char* label;
				frame->queryGet(i, &t, &label);
				if (label) {
					entry_stack.push_back(entries.size());
					entries.push_back(GPUTimerGUIEntry{ label,t,0,(int)entry_stack.size()-1,false});
				}
				if (!label) {
					assert(entry_stack.size());
					unsigned e = (int)entry_stack.back();
					entry_stack.pop_back();
					entries[e].end_time = t;
					entries[e].is_leaf = e == (entries.size()-1);
				}
			}

			ImDrawList* dl = gui::GetWindowDrawList();
			const vec2 cp = gui::GetCursorScreenPos();
			const vec2 cs = gui::GetContentRegionAvail();

			float width = 15.f;
			const vec2 ur0 = cp + vec2(cs.x, 0);
			const vec2 ll0 = ur0 - vec2(width, -cs.y);
			/* XXX 
                           vec3 bg_col = vec3(0.4f);
			   dl->AddRectFilled(ll0, ur0, gui::ColorConvertFloat4ToU32(vec4(bg_col, 1.f)));
                        */
			
			float t_min = ur0.y;
			float t_max = ll0.y-10.f;
			float w_min = ll0.x;
			float w_max = ur0.x;

			u64 t_base = entries[0].start_time;
			u64 t_total = entries[0].end_time - entries[0].start_time;
			double invfreq = 1.0 / (1000000000.0);
			//t_total = (1.f / 90.f) / invfreq;
			for (int i = 0; i < (int)entries.size(); ++i) {
				const auto& e = entries[i];
				vec3 col = getColor(i, e);
				double t_elapsed = (e.end_time - e.start_time)*invfreq*1000.0;
				float t_start = (e.start_time - t_base) / double(t_total);
				float t_end = (e.end_time - t_base) / double(t_total);
				vec2 ll = vec2(w_min, lerp(t_min, t_max, t_end));
				vec2 ur = vec2(w_max, lerp(t_min, t_max, t_start));
                                /* XXX unused		bool hovered = false; */

				vec2 mp = gui::GetMousePos();
				bool hovered_bar = e.is_leaf &&  mp >= vec2(ll.x, ur.y) && mp <= vec2(ur.x,ll.y);
		
				gui::PushStyleColor(ImGuiCol_Text, vec4(col * (hovered_bar ? 2.f : 1.f), 1.f));
				
				if (e.depth) gui::Indent(gui::GetStyle().IndentSpacing*e.depth/2);
				//gui::Text("%s: %3.2fms", e.label, t_elapsed);
				
				gui::Text("%s", e.label);
				//gui::Selectable(e.label, false);
				bool hovered_text = gui::IsItemHovered();
				if (e.depth) gui::Unindent(gui::GetStyle().IndentSpacing*e.depth/2);
				float cpx = gui::GetCursorPosX();
				//if (e.is_leaf) {
					gui::SameLine();
					gui::SetCursorPosX(160);
					gui::Text("%5.2f", t_elapsed);
					hovered_text |= gui::IsItemHovered();
					gui::SetCursorPosX(cpx);
				//}
				gui::PopStyleColor(1);
				
				if (hovered_text || hovered_bar) {
					col *= 2.f;
					ll.x -= 3.f;
					ur.x += 3.f;
				}
				
				if (e.is_leaf || hovered_text) {

					dl->AddRectFilled(ll, ur, gui::ColorConvertFloat4ToU32(vec4(col, 1.f)));
				}
			}
			/*
			u64 t;
			const char* label;
			frame->queryGet(0, &t, &label); // frame is guaranteed to have at least 1 result
			u64 base = t;
			double invfreq = 1.f / (1000000000);
			for (int i = 0; i < frame->count; ++i) {
				frame->queryGet(i, &t, &label);
				u64 s = t - base;
				if (!label)
					gui::Unindent();
				gui::Text("%s: %llu %fms", label ? label : "", s, s*invfreq*1000.0);
				if (label)
					gui::Indent();
			}
			*/
		}
	} gui::End();
}

float getTimeOfLabel(GPUCounterFrame* frame, const char* name) {
	if (frame) {
		u64 start = 0;
		u64 end = 0;
		int stack = 0;
		int start_stack = 0;
		for (int i = 0; i < frame->count; ++i) {
			u64 t;
			const char* label;
			frame->queryGet(i, &t, &label);
			if (label) {
				stack += 1;
				if (strcmp(label, name) == 0) {
					start = t;
					start_stack = stack;
				}
			}
			if (!label) {
				if (stack == start_stack) {
					end = t;
					break;
				}
				stack -= 1;
				assert(stack >= 0);
			}
		}
		double invfreq = 1.0 / (1000000000.0);
		double t_elapsed = (end - start)*invfreq;
		return t_elapsed;
	}
	return 0.f;
}
