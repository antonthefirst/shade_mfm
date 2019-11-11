#include "timers.h"
#include "core/cpu_timer.h"
#include "core/gpu_timer.h"
#include "imgui/imgui.h"
#include "wrap/input_wrap.h"


struct TimerHistory;

struct TimerHashEntry {
	HashedString hs;
	TimerHashEntry* next;
	s64 t_start;
	s64 t_sum;
	int hits;
};

struct HistoryHashEntry {
	u32 hash;
	HistoryHashEntry* next;
	HistoryHashEntry* prev;

	TimerHistory* hist_ptr;
};

template<typename T, int MAX_COUNT>
struct BinIndexed {
	T   ptr[MAX_COUNT];
	T*  free_ptr[MAX_COUNT];
	int free_count = 0;
	
	void clear() {
		for (int i = 0; i < MAX_COUNT; ++i)
			free_ptr[i] = ptr + (MAX_COUNT - 1 - i);
		free_count = MAX_COUNT;
	}
	void clear_and_zero() {
		clear();
		zero();
	}
	T* add() {
		if (free_count > 0) {
			T* v = free_ptr[--free_count];
			return v;
		}
		else {
			return 0;
		}
	}
	T* add(const T& val) {
		if (T* v = add()) {
			*v = val;
			return v;
		}
		else {
			return 0;
		}
	}
	void rem(T* val) {
		if (free_count < MAX_COUNT) {
			free_ptr[free_count++] = val;
		}
	}
	size_t bytes_of_data() {
		return sizeof(T) * MAX_COUNT;
	}
	void zero() {
		memset(ptr, 0, bytes_of_data());
	}
};

#define TIMER_HISTORY_LENGTH 128 // keep power of two for % efficiency sake
struct TimerHistory {
	int sample_idx;
	float samples[TIMER_HISTORY_LENGTH];
	bool used;
	bool touched;
	HistoryHashEntry key;
	void add(float s) {
		samples[(sample_idx++)%TIMER_HISTORY_LENGTH] = s;
		touched = true;
	}
	void stats(float* avg_out, float* min_out, float* max_out) {
		float sum = 0.0f;
		float min_val = FLT_MAX;
		float max_val = 0.0f;
		for (int i = 0; i < TIMER_HISTORY_LENGTH; ++i) {
			sum += samples[i];
			min_val = min(min_val, samples[i]);
			max_val = max(max_val, samples[i]);
		}
		*avg_out = sum / float(TIMER_HISTORY_LENGTH);
		*min_out = min_val;
		*max_out = max_val;
	}
};


template<typename T, int MAX_ENTRIES>
struct HashSimple {
	static const int CAPACITY = MAX_ENTRIES * 2;
	T table[CAPACITY];
	BinIndexed<T, MAX_ENTRIES> block;
	HashSimple() {
		clear();
	}
	void clear() {
		memset(table, 0, sizeof(table));
		block.clear_and_zero();
	}
	T* getInternal(T key, T** last_found) {
		int i = key.hash % CAPACITY;
		T* e = &table[i];
		while (e) {
			if (e->prev == 0) { // blank entry
				*last_found = e;
				return 0;
			}
			else if (e->hash == key.hash) { // hash matches
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
	T* get(T key) {
		T* last_found;
		return getInternal(key, &last_found);
	}
	T* getOrAdd(T key, bool* added) {
		T* last_found;
		if (T* found = getInternal(key, &last_found)) {
			*added = false;
			return found;
		}
		else {
			*added = true;
			if (last_found->prev == 0) { // blank entry
				*last_found = key;
				last_found->prev = last_found;
				last_found->next = 0;
				return last_found;
			}
			else {
				assert(last_found->next == 0);
				if ( (last_found->next = block.add(key)) ) { // make a new entry
					last_found->next->prev = last_found;
					last_found->next->next = 0;
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
	void rem(T key) {
		if (T* found = get(key)) {
			if (found->prev != found) { // means we're inside the block alloc
				if (found->next) {
					// tie up prev and next, then remove ourselves
					found->prev->next = found->next;
					found->next->prev = found->prev;		
				}
				else {
					// zero out our connection from prev
					found->prev->next = 0;
				}
				// remove ourselves
				block.rem(found);
			}
			else {
				if (found->next) {
					// we have a next, so have to move it into the main table
					T* next = found->next;
					*found = *next;
					block.rem(next);
					found->prev = found; // complete the move by setting prev to be circular
				}
				else {
					// no next, so just reset ourselves to being blank
					found->prev = 0;
				}
			}
		}
	}
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


#include <vector>

struct Timer {
	TimerHash table;
	std::vector<u32> hash_stack;
	std::vector<HashedString> stack;
	std::vector<HashedString> order;
	std::vector<int> depth;
	BinIndexed<TimerHistory, TIMER_COUNT_MAX> histories;
	HashSimple<HistoryHashEntry, TIMER_COUNT_MAX> history_hash;

	s64 t_overhead_sum = 0;
	s64 t_overhead_start = 0;

	Timer() {
		reset();
	}
	void reset() {
		stack.reserve(TIMER_COUNT_MAX);
		order.reserve(TIMER_COUNT_MAX);
		depth.reserve(TIMER_COUNT_MAX);
		histories.clear_and_zero();
		history_hash.clear();
	}
	void new_frame() {
		t_overhead_start = time_counter();
		assert(stack.size() == 0);
		table.clear();
		stack.clear();
		order.clear();
		depth.clear();
		hash_stack.push_back(WangHash(0xafe38781));
		t_overhead_sum = time_counter() - t_overhead_start;
	}
	void start(HashedString hs) {
		t_overhead_start = time_counter();
		hs.hash = WangHash(hash_stack.back() ^ hs.hash);
		hash_stack.push_back(hs.hash);
		bool added;
		if (TimerHashEntry* e = table.getOrAdd(hs, &added)) {
			if (added) {
				order.push_back(hs);
				depth.push_back((int)stack.size());
			}
			stack.push_back(hs);
			e->t_start = time_counter();
		}
		t_overhead_sum += time_counter() - t_overhead_start;
	}
	void stop() {
		if (stack.size() == 0) return;
		t_overhead_start = time_counter();
		s64 t_end = time_counter();
		if (TimerHashEntry* e = table.get(stack.back())) {
			e->t_sum += t_end - e->t_start;
			e->hits += 1;
			stack.pop_back();
			hash_stack.pop_back();
		}
		else {
			assert(false);
		}
		t_overhead_sum += time_counter() - t_overhead_start;
	}
	void printLog() {
		assert(order.size() == depth.size());
		for (unsigned i = 0; i < order.size(); ++i) {
			for (int t = 0; t < depth[i]; ++t) {
				log("  ");
			}
			if (TimerHashEntry* e = table.get(order[i])) {
				log("%s: sum %lld, hits %d\n", e->hs.str, e->t_sum, e->hits);
			}
			else {
				log("Error!\n");
				assert(false);
			}
		}
	}
	void printGui() {
		s64 t_gui_start = time_counter();
		assert(order.size() == depth.size());
		for (int i = 0; i < TIMER_COUNT_MAX; ++i) {
			if (histories.ptr[i].used)
				histories.ptr[i].touched = false;
		}
		for (unsigned i = 0; i < order.size(); ++i) {
			if (depth[i] != 0) gui::Indent(depth[i]*gui::GetStyle().IndentSpacing + 1.0f);
			if (TimerHashEntry* e = table.get(order[i])) {
				
				float t_sum = time_to_msec(e->t_sum);
				float t_sum_avg = 0.0f;
				float t_sum_min = 0.0f;
				float t_sum_max = 0.0f;
				HistoryHashEntry hist_key;
				hist_key.hash = e->hs.hash;
				bool added;
				
				if (HistoryHashEntry* hist_entry = history_hash.getOrAdd(hist_key, &added)) {
					if (added) {
						hist_entry->hist_ptr = histories.add();
						hist_entry->hist_ptr->key = hist_key;
						hist_entry->hist_ptr->used = true;
					}
					hist_entry->hist_ptr->add(t_sum);
					hist_entry->hist_ptr->stats(&t_sum_avg, &t_sum_min, &t_sum_max);
				}
				/*
				if (e->hits == 1)
					gui::Text("%s: %3.3f (%3.3f +- %3.3f)", e->hs.str, t_sum, t_sum_avg, (t_sum_max - t_sum_min)*0.5f);
				else
					gui::Text("%s: %3.3f (%3.3f +- %3.3f) /%d = %3.3f", e->hs.str, t_sum, t_sum_avg, (t_sum_max - t_sum_min)*0.5f, e->hits, t_sum / e->hits);
				*/
				if (e->hits == 1) {
					gui::Text("%s: %3.3f +- %3.3f", e->hs.str, t_sum_avg, (t_sum_max - t_sum_min)*0.5f);
					if (gui::IsItemHovered()) {
						gui::BeginTooltip();
						gui::Text("%3.3f", t_sum);
						gui::EndTooltip();
					}
				}
				else {
					gui::Text("%s: %3.3f +- %3.3f /%d = %3.3f", e->hs.str, t_sum_avg, (t_sum_max - t_sum_min)*0.5f, e->hits, t_sum_avg / e->hits);	
					if (gui::IsItemHovered()) {
						gui::BeginTooltip();
						gui::Text("%3.3f", t_sum);
						gui::EndTooltip();
					}
				}
			}
			
			else {
				log("Error!\n");
				assert(false);
			}

			if (depth[i] != 0) gui::Unindent(depth[i] * gui::GetStyle().IndentSpacing + 1.0f);
		}
		for (int i = 0; i < TIMER_COUNT_MAX; ++i) {
			if (histories.ptr[i].used && !histories.ptr[i].touched) {
				history_hash.rem(histories.ptr[i].key);
				memset(&histories.ptr[i], 0, sizeof(TimerHistory));
				histories.rem(histories.ptr + i);
			}
		}
		gui::Separator();
		s64 t_gui_overhead = time_counter() - t_gui_start;
		gui::Text("Overhead: %3.3f (%lld), gui %3.3f (%lld) ", time_to_msec(t_overhead_sum), t_overhead_sum, time_to_msec(t_gui_overhead), t_gui_overhead);

		//table.dev_stats();
	}
};


static Timer ctimer;
static TimerResult timer_results[TIMER_TREE_MAX_TIMERS];
static GPUTimer gpu_timer;
static bool cpu_timer_suspended = false;

/*
void ctimer_reset() {
	ctimer.new_frame();
}
void chashtimer_start(HashedString hs) {
	ctimer.start(hs);
}
void chashtimer_stop() {
	ctimer.stop();
}
*/

void gtimer_reset() {
	gpu_timer.reset();
}
void gtimer_start(const char* name) {
	gpu_timer.start(name);
}
void gtimer_stop() {
	gpu_timer.stop();
}

void timerUI(Input& in) {
	static bool show_timer = false;
	if (in.key.press[KEY_F1])
		show_timer = !show_timer;

	if (show_timer) {
		if (gui::Begin("cpu timer")) {
			ctimer.printGui();
		} gui::End();

		GPUCounterFrame* frame = gpu_timer.read();
		guiGPUTimer(frame, "gpu timer");
	}
}

