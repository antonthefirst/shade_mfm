#include "timers.h"
#include "core/basic_types.h"
#include "core/log.h"
#include "core/maths.h"
#include "wrap/input_wrap.h"
#include "core/shader_loader.h"
#include "core/dir.h"
#include "core/gpu_timer.h"
#include "core/cpu_timer.h"

#include "shaders/cpu_gpu_shared.inl"
#include "shaders/defines.inl" // for RADIUS
//#include "shaders/splitmix32.inl"
#include "splat_compiler.h"
#include "world.h"
#include "render.h"
#include "compute.h"

#include "wrap/evk.h"
#include <GLFW/glfw3.h>
#include <imgui/imgui.h>

#define INSPECT_MODE_BASIC 0
#define INSPECT_MODE_FULL 1

#define NO_SITE ivec2(-1);

namespace {

struct SimControl {
	bool do_reset = false;
	int dispatches_per_batch = 0;
	int dispatch_counter = 0;
	int stop_at_n_dispatches = 0;
} ctrl;

}

// mfm state
static GLuint ctrl_state_handle = 0;
static bool reset_ok = false;
static ProgramInfo prog_info;
static World world;

// stats state
#define STATS_BUFFER_SIZE 2
static GLuint stats_handles[STATS_BUFFER_SIZE];
static WorldStats stats;
static GLuint site_info_handles[STATS_BUFFER_SIZE];
static SiteInfo site_info;
static int stats_write_idx = 0;
static ivec2 site_info_idx = ivec2(0);
static bool hold_site_info_idx = false;
static bool want_stats = true;


// render state
static GLuint unit_quad_vao;
static GLuint unit_quad_vbo;
static GLuint unit_quad_ebo;
//static bool do_update = false;
static bool run = false;
static int update_rate_custom = 16;
static int update_rate_option = 1;
static pose camera_from_world = identity();
static pose camera_from_world_start = identity();
static pose camera_from_world_target = identity();

// gui state
static ivec2 gui_world_res = ivec2(256);
static int gui_res_option = 7;
static GPUTimer update_timer;
static bool open_shader_gui = false;
static s64 time_of_reset = 0;
static s64 events_since_reset = 0;
static double sim_time_since_reset = 0.0;
static double wall_time_since_reset = 0.0;
static float scale_before_zoomout = 1.0f;
static bool is_overview = false;
static float mouse_wheel_smooth = 0.0f;
static int overview_state = 0;
static float overview_transition_timer = -1.0f;
static float AER_history[30];
static u32 AER_history_idx = 0;
static bool show_zero_counts = 0;
static int inspect_mode = INSPECT_MODE_BASIC;
static ProgramStats prog_stats;
static bool dont_reset_when_code_changes = false;
static bool gui_break_on_event = false;
static float event_window_vis = 0.0f;


static void initStatsIfNeeded() {
	/* #PORT
	// it sounds like these should really be glBufferStorage, but that requires 4.4
	// https://www.khronos.org/opengl/wiki/Buffer_Object#Immutable_Storage
	// https://stackoverflow.com/questions/36239869/performance-warning-when-using-glmapbufferrange
	// https://stackoverflow.com/questions/27810542/what-is-the-difference-between-glbufferstorage-and-glbufferdata
	if (stats_handles[0] == 0) {
		WorldStats zero_stats;
		memset(&zero_stats, 0, sizeof(WorldStats));
		for (unsigned i = 0; i < ARRSIZE(stats_handles); i++) {
			glGenBuffers(1, &stats_handles[i]);
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, stats_handles[i]);
			glObjectLabel(GL_BUFFER, stats_handles[i], -1, "'stats_handles'");
			glBufferData(GL_SHADER_STORAGE_BUFFER, (GLsizeiptr)(sizeof(WorldStats)), &zero_stats, GL_STREAM_READ);
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
		}
	}
	if (site_info_handles[0] == 0) {
		SiteInfo zero_info;
		memset(&zero_info, 0, sizeof(SiteInfo));
		for (unsigned i = 0; i < ARRSIZE(site_info_handles); i++) {
			glGenBuffers(1, &site_info_handles[i]);
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, site_info_handles[i]);
			glObjectLabel(GL_BUFFER, site_info_handles[i], -1, "'site_info_handles'");
			glBufferData(GL_SHADER_STORAGE_BUFFER, (GLsizeiptr)(sizeof(SiteInfo)), &zero_info, GL_STREAM_READ);
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
		}
	}
	//#TODO this probably needs to live elsewhere...
	if (ctrl_state_handle == 0) {
		ControlState zero_ctrl;
		memset(&zero_ctrl, 0, sizeof(ControlState));
		glGenBuffers(1, &ctrl_state_handle);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, ctrl_state_handle);
		glObjectLabel(GL_BUFFER, ctrl_state_handle, -1, "'ctrl_state_handle'");
		glBufferData(GL_SHADER_STORAGE_BUFFER, (GLsizeiptr)(sizeof(ControlState)), &zero_ctrl, GL_STATIC_DRAW);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
	}
	*/
}
static void zeroControlSignals() {
	/* #PORT
	// Site Info contains the break flag right now, maybe move it elsewhere
	SiteInfo zero_info;
	memset(&zero_info, 0, sizeof(SiteInfo));
	for (unsigned i = 0; i < ARRSIZE(site_info_handles); i++) {
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, site_info_handles[i]);
		glBufferData(GL_SHADER_STORAGE_BUFFER, (GLsizeiptr)(sizeof(SiteInfo)), &zero_info, GL_STREAM_READ);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
	}
	ControlState zero_ctrl;
	memset(&zero_ctrl, 0, sizeof(ControlState));
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, ctrl_state_handle);
	glBufferData(GL_SHADER_STORAGE_BUFFER, (GLsizeiptr)(sizeof(ControlState)), &zero_ctrl, GL_STATIC_DRAW);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
	*/
}
static void mfmSetUniforms(int stage = 0) {
	/* #PORT
	glBindImageTexture(0, site_bits_img, 0, GL_FALSE, 0, GL_READ_WRITE, SITE_FORMAT);
	glUniform1i(0, 0);
	glBindImageTexture(1, color_img, 0, GL_FALSE, 0, GL_WRITE_ONLY, COLOR_FORMAT);
	glUniform1i(1, 1);
	glBindImageTexture(2, dev_img, 0, GL_FALSE, 0, GL_READ_WRITE, DEV_FORMAT);
	glUniform1i(2, 2);
	glBindImageTexture(3, vote_img, 0, GL_FALSE, 0, GL_READ_WRITE, VOTE_FORMAT);
	glUniform1i(3, 3);
	glBindImageTexture(4, event_count_img, 0, GL_FALSE, 0, GL_READ_WRITE, EVENT_COUNT_FORMAT);
	glUniform1i(4, 4);
	glBindImageTexture(5, prng_state_img, 0, GL_FALSE, 0, GL_READ_WRITE, PRNG_STATE_FORMAT);
	glUniform1i(5, 5);
	glUniform1ui(6, dispatch_counter);
	glUniform2i(7, site_info_idx.x, site_info_idx.y);
	glUniform1i(8, stage);
	glUniform1i(9, gui_break_on_event);
	glUniform1i(10, 0); // hacky constants to prevent gpu loop unrolling
	glUniform1i(11, 1);
	glUniform1i(12, EVENT_WINDOW_RADIUS*2);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, stats_handles[stats_write_idx]);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, site_info_handles[stats_write_idx]);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, ctrl_state_handle);
	*/
}

static void mfmComputeStats(ivec2 world_res) {
	/* #PORT
	mfmSetUniforms(STAGE_COMPUTE_STATS);
	gtimer_start("stats");
	glDispatchCompute((world_res.x / GROUP_SIZE_X) + 1, (world_res.y / GROUP_SIZE_Y) + 1, 1);
	gtimer_stop();
	glMemoryBarrier(
		GL_SHADER_STORAGE_BARRIER_BIT      |
		GL_SHADER_IMAGE_ACCESS_BARRIER_BIT |
		GL_TEXTURE_FETCH_BARRIER_BIT          // probably not needed
	);
	*/
}
static void mfmSiteInfo(ivec2 world_res) {
	/* #PORT
	mfmSetUniforms(STAGE_SITE_INFO);
	gtimer_start("site info");
	glDispatchCompute(1, 1, 1);
	gtimer_stop();
	glMemoryBarrier(
		GL_SHADER_STORAGE_BARRIER_BIT      |
		GL_SHADER_IMAGE_ACCESS_BARRIER_BIT |
		GL_TEXTURE_FETCH_BARRIER_BIT          // probably not needed
	);
	*/
}
static void mfmClearStats() {
	/* #PORT
	mfmSetUniforms(STAGE_CLEAR_STATS);
	gtimer_start("clear stats");
	glDispatchCompute(1, 1, 1);
	//glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT); // opt
	glMemoryBarrier(GL_ALL_BARRIER_BITS); // safe
	gtimer_stop();
	*/
}
static void mfmReadStats() {
	/* #PORT
	gtimer_start("read stats");
	int stats_read_idx = 1 - stats_write_idx;
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, stats_handles[stats_read_idx]);
	glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(WorldStats), &stats);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, site_info_handles[stats_read_idx]);
	glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(SiteInfo), &site_info);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
	events_since_reset += stats.event_count_this_batch;
	stats_write_idx = stats_read_idx;
	gtimer_stop();
	*/
}

static void dirscanCallback(const char* pathfile, const char* name, const char* ext) {
	gui::Selectable(name, false);
} 
static pose calcOverviewPose(ivec2 world_res) {
	vec2 w = vec2(world_res);
	float s = 2.0f / w.y * 0.9f; // "a little bit zoomed out"
	return pose(-w.x * 0.5 * s, -w.y * 0.5 * s , s, 0.0f);
}
static pose zoomIncrementalCenteredOnPoint(pose camera_from_world, vec2 camera_from_point, float zoom_amount) {
	float curr_scale = scaleof(camera_from_world);
	float new_scale = exp(log(curr_scale) + zoom_amount);
	return trans(camera_from_point) * scale(new_scale / curr_scale) * trans(-camera_from_point);
}
static pose zoomInstantCenteredOnPoint(pose camera_from_world, vec2 camera_from_point, float new_scale) {
	float curr_scale = scaleof(camera_from_world);
	return trans(camera_from_point) * scale(new_scale / curr_scale) * trans(-camera_from_point) *  camera_from_world;
}

struct GuiSettings {
	bool enable_break_at_step = false;
	int break_at_step_number = 1000;
	int run_speed = 1;
	int size_option = 7;
	ivec2 size_custom = ivec2(80, 60);
};

static GuiSettings gui_set;

static void guiControl(bool* reset, bool* run, bool* step, ivec2* size_request, int dispatch_counter, float AEPS, float AER) {
	bool size_change = false;
	gui::Text("Size:");
	size_change |= gui::RadioButton("64x64     ", &gui_set.size_option,  6); gui::SameLine();
	size_change |= gui::RadioButton("128x128   ", &gui_set.size_option,  7); gui::SameLine();
	size_change |= gui::RadioButton("256x256   ", &gui_set.size_option,  8); gui::SameLine();
	size_change |= gui::RadioButton("512x512   ", &gui_set.size_option,  9); 
	size_change |= gui::RadioButton("1024x1024 ", &gui_set.size_option, 10); gui::SameLine();
	size_change |= gui::RadioButton("2048x2048 ", &gui_set.size_option, 11); gui::SameLine();
	size_change |= gui::RadioButton("4096x4096 ", &gui_set.size_option, 12); gui::SameLine();
	size_change |= gui::RadioButton("8192x8192 ", &gui_set.size_option, 13); 
	size_change |= gui::RadioButton("Custom:",     &gui_set.size_option, 0); gui::SameLine();
	size_change |= gui::InputInt2("##size_custom", (int*)&gui_set.size_custom);
	if (gui_set.size_option != 0) {
		*size_request = ivec2(1 << gui_set.size_option);
	}
	else {
		if (gui_set.size_custom.x > 0 && gui_set.size_custom.y > 0)
			*size_request = gui_set.size_custom;
	} 

	if (size_change) {
		//*run = false;
		*reset = true;
	}
	
	if (gui_set.enable_break_at_step && dispatch_counter == (unsigned)gui_set.break_at_step_number) 
		*run = false;

	gui::Separator();
	
	gui::Columns(2, 0, true);
	gui::SetColumnWidth(0, 300.0f);
	gui::SetCursorPosX(30);
	if (gui::Button("Reset and Run")) {
		*reset = true;
		*run = true;
	} gui::SameLine();
	if (gui::Button("Reset")) {
		*reset = true;
		*run = false;
	} gui::SameLine();

	if (*run) {
		if (gui::Button("Break")) {
			*run = false;
		}
	} else {
		if (gui::Button(" Run ")) {
			*run = true;
		}
	}
	gui::SameLine();

	// #SUBTLE don't want holding down Step button to blow past break counter
	if (!gui_set.enable_break_at_step || dispatch_counter != (unsigned)gui_set.break_at_step_number-1)
		gui::PushButtonRepeat(true);
	if (gui::Button("Step")) {
		*step = true;
		*run = false;
		if (dispatch_counter == (unsigned)gui_set.break_at_step_number)
			gui_set.enable_break_at_step = false;
	}
	if (!gui_set.enable_break_at_step || dispatch_counter != (unsigned)gui_set.break_at_step_number-1)
		gui::PopButtonRepeat();

	
	/* #PORT
	gui::AlignFirstTextHeightToWidgets();
	*/
	gui::Text("Speed:"); gui::SameLine();
	gui::SetCursorPosX(55);
	gui::RadioButton("1  ",   &gui_set.run_speed, 1); gui::SameLine();
	gui::RadioButton("2  ",   &gui_set.run_speed, 2); gui::SameLine();
	gui::RadioButton("4  ",   &gui_set.run_speed, 4); gui::SameLine();
	gui::RadioButton("8  ",   &gui_set.run_speed, 8); gui::SameLine();
	gui::RadioButton("16 ",  &gui_set.run_speed, 16);
	gui::SetCursorPosX(55);
	gui::RadioButton("32 ",  &gui_set.run_speed, 32); gui::SameLine();
	gui::RadioButton("64 ",  &gui_set.run_speed, 64); gui::SameLine();
	gui::RadioButton("128", &gui_set.run_speed, 128); gui::SameLine();
	gui::RadioButton("256", &gui_set.run_speed, 256); gui::SameLine();
	gui::RadioButton("512", &gui_set.run_speed, 512);

	if (gui_set.enable_break_at_step) {
		gui::Checkbox("Break at step number:", &gui_set.enable_break_at_step); gui::SameLine();
		gui::PushItemWidth(100.0f);
		gui::InputInt("##break_at_step_number", &gui_set.break_at_step_number);
		gui::PopItemWidth();
		//gui::Text("Current: %5d", dispatch_counter);
	}
	else {
		gui::Checkbox("Break at step...", &gui_set.enable_break_at_step); 
	}

	gui_set.break_at_step_number = max(1, gui_set.break_at_step_number);

	gui::Checkbox("Break on event", &gui_break_on_event);

	gui::Checkbox("Don't reset when code changes", &dont_reset_when_code_changes);

	gui::NextColumn();

	if (AEPS < 1000.0)
		gui::Text("AEPS: %.1f", AEPS);
	else
		gui::Text("AEPS: %.3fK", AEPS/1000.0);
	if (gui::IsItemHovered()) gui::SetTooltip("Average Events Per Site");
	
	gui::Text("AER:  %0.3f", AER);
	if (gui::IsItemHovered()) gui::SetTooltip("Average Event Rate (AEPS/sec)");
	
	gui::Text("Step: %d", dispatch_counter);
	if (gui::IsItemHovered()) gui::SetTooltip("Steps since last reset");

	gui::Columns();

}

void guiAtomInspector(Atom A) {
	u32 atom_type = unpackBits(A, ATOM_TYPE_GLOBAL_OFFSET, ATOM_TYPE_BITSIZE);
	if (atom_type >= prog_info.elems.count) {
		gui::Text("Unknown or corrupt type %u\n", atom_type);
		return;
	}

	const ElementInfo* prog = &prog_info.elems[atom_type];
	if (inspect_mode == INSPECT_MODE_BASIC)
		gui::Text("Type: [%-2.*s] %.*s", prog->symbol.len, prog->symbol.str, prog->name.len, prog->name.str);
	if (inspect_mode == INSPECT_MODE_FULL)
		gui::Text("%.*s atom (type %08x)", prog->name.len, prog->name.str, atom_type);

	int color_idx = 0;
	vec4 color_choices[] = {vec4(1.0f,0.7f,0.7f,1.0f), vec4(0.7f,1.0f,0.7f,1.0f)};
	int prev_field_idx = -1;
	static String text;

	if (inspect_mode == INSPECT_MODE_BASIC) {		
		gui::Text("Data: %s", prog->data.count == 2 || prog->data.count == 0 ? "None" : ""); // 2 fields = TYPE and ECC
		for (int d = 0; d < prog->data.count; ++d) {
			const DataField& D = prog->data[d];
			if (D.global_offset == ATOM_ECC_GLOBAL_OFFSET || D.global_offset == ATOM_TYPE_GLOBAL_OFFSET) // skip hidden data
				continue; 
			text.clear();
			D.appendTo(A, text);
			gui::Text("  %.*s", text.len, text.str);
		}
	}
	if (inspect_mode == INSPECT_MODE_FULL) {
		for (int i = 0; i < ATOM_BITS; ++i) {
			text.clear();
			vec4 col = vec4(0.3f, 0.3f, 0.3f, 1.0f);
			for (int d = 0; d < prog->data.count; ++d) {
				const DataField& D = prog->data[d];
				if (D.global_offset == NO_OFFSET) continue;
				if (i >= D.global_offset && i < (D.global_offset + D.bitsize)) {
					if (prev_field_idx == -1)
						prev_field_idx = d;
					if (i == D.global_offset)  {
						D.appendTo(A, text);
					}
					if (prev_field_idx != d) {
						color_idx = (color_idx+1)%ARRSIZE(color_choices);
						prev_field_idx = d;
					}
					col = color_choices[color_idx];
					break;
				}
			}
			int b = unpackBits(A, i, 1);
			gui::PushStyleColor(ImGuiCol_Text, col);
			gui::Text("%d %02d %03d: %d %.*s", i/BITS_PER_COMPONENT, i%BITS_PER_COMPONENT, i, b, text.len, text.str);
			gui::PopStyleColor(1);
		}
	}
}

void mfmInit() {

}
void mfmTerm() {
	world.destroy();
	computeDestroy();
	renderDestroy();
}

void mfmUpdate(Input* main_in) {
	ctimer_start("update");
	computeRecreatePipelineIfNeeded();
	renderRecreatePipelineIfNeeded();

	ctimer_start("controls");
	ivec2 screen_res = ivec2(evk.win.Width, evk.win.Height);
	
	ctrl.do_reset = false;
	bool do_step = false;

	bool mouse_is_onscreen = main_in->mouse.x >= 0 && main_in->mouse.x <= 1.0f && main_in->mouse.y >= 0 && main_in->mouse.y <= 1.0f; 
	bool mouse_is_using_ui = /* #PORT gui::IsMouseHoveringAnyWindow() || */ gui::IsAnyItemActive();
	float camera_aspect = float(evk.win.Width) / float(evk.win.Height);
	vec2 camera_from_mouse = (vec2(main_in->mouse.x, main_in->mouse.y)*2.0f - vec2(1.f)) * vec2(camera_aspect, 1.f);
	vec2 world_from_mouse = ~camera_from_world * camera_from_mouse;
	ivec2 hover_site_idx = ivec2(world_from_mouse);
	hover_site_idx.y = gui_world_res.y - 1 - hover_site_idx.y; //#Y-DOWN

#define OVERVIEW_INACTIVE 0
#define OVERVIEW_ZOOMING_IN 1
#define OVERVIEW_ZOOMING_OUT 2

	mouse_wheel_smooth = lerp(mouse_wheel_smooth, main_in->mouse.wheel, 0.2f);
	pose mouse_zoom_diff = identity();
	pose mouse_pan_diff = identity();
	if (mouse_is_onscreen && !mouse_is_using_ui) {
		mouse_zoom_diff = zoomIncrementalCenteredOnPoint(camera_from_world_target, camera_from_mouse, mouse_wheel_smooth * 0.1f);
			
		if (main_in->mouse.down[MOUSE_BUTTON_2]) {
			mouse_pan_diff = trans(vec2(main_in->mouse.dx, main_in->mouse.dy) * vec2(camera_aspect, 1.f) * 2.0f);
		}
		if (main_in->mouse.press[MOUSE_BUTTON_3]) { 
			pose camera_from_world_overview = calcOverviewPose(gui_world_res);
			pose camera_from_world_focus = zoomInstantCenteredOnPoint(camera_from_world, camera_from_mouse, scale_before_zoomout);
			camera_from_world_start = camera_from_world;
			overview_transition_timer = 0.0f;
			if (overview_state == OVERVIEW_INACTIVE) {
				float scale_diff = exp(abs(log(scaleof(camera_from_world)) - log(scaleof(camera_from_world_overview))));
				if (scale_diff >= 1.01f) { // if we're not near overview scale, go to overview
					scale_before_zoomout = scaleof(camera_from_world); 
					camera_from_world_target = camera_from_world_overview;
					overview_state = OVERVIEW_ZOOMING_OUT;
				} else {
					camera_from_world_target = camera_from_world_focus;
					overview_state = OVERVIEW_ZOOMING_IN;
				}
			} else if (overview_state == OVERVIEW_ZOOMING_IN) {
				camera_from_world_target = camera_from_world_overview;
				overview_state = OVERVIEW_ZOOMING_OUT;
			} else { // zooming out
				camera_from_world_target = camera_from_world_focus;
				overview_state = OVERVIEW_ZOOMING_IN;
			}
		}
		if (hover_site_idx >= ivec2(0,0) && hover_site_idx < gui_world_res) {
			if (main_in->mouse.press[MOUSE_BUTTON_1])
				hold_site_info_idx = !hold_site_info_idx;
		}
	}

	if (!hold_site_info_idx)
		site_info_idx = hover_site_idx;
	
	const float OVERVIEW_TRANSITION_TIME = 0.5f;
	if (overview_transition_timer >= 0.0f) {
		float a = clamp(overview_transition_timer / OVERVIEW_TRANSITION_TIME, 0.0f, 1.0f);
		camera_from_world = lerp(camera_from_world_start, camera_from_world_target, smoothstep(a));
		if (a == 1.0f) {
			overview_transition_timer = -1.0f;
			overview_state = OVERVIEW_INACTIVE;
		}
		else
			overview_transition_timer += 1.0f / 60.0f;
	} else {
		camera_from_world = mouse_zoom_diff * camera_from_world;
		camera_from_world = mouse_pan_diff * camera_from_world;
	}

	ctimer_stop(); // controls

	ctimer_start("stats"); //

	static s64 time_of_frame_start = 0;
	float sec_per_frame = run ? time_to_sec(time_counter() - time_of_frame_start) : 0.0f;
	time_of_frame_start = time_counter();

	GPUCounterFrame* gpu_frame = update_timer.read();
	float sec_per_batch = getTimeOfLabel(gpu_frame, "batch");
	if (run) {
		sim_time_since_reset += sec_per_batch;
		wall_time_since_reset += sec_per_frame;
	}

	ctrl.dispatches_per_batch = 0;

	// compute AEPS and AER
	int total_sites = gui_world_res.x * gui_world_res.y;
	double AEPS = double(events_since_reset) / double(total_sites);
	float events_per_sec = stats.event_count_this_batch / sec_per_batch;
	float AER = sec_per_batch == 0.0f ? 0.0f : events_per_sec / total_sites;
	float AER_avg = 0.0f;
	for (int i = 0; i < ARRSIZE(AER_history); ++i)
		AER_avg += AER_history[i];
	AER_avg /= ARRSIZE(AER_history);
	
	if (gui::Begin("Control")) {
		guiControl(&ctrl.do_reset, &run, &do_step, &gui_world_res, ctrl.dispatch_counter, AEPS, AER_avg);
	} gui::End();

	if (gui::Begin("Visualize")) {
		gui::SliderFloat("event windows", &event_window_vis, 0.0f, 1.0f);
	} gui::End();

	if (run) {
		ctrl.dispatches_per_batch = gui_set.run_speed;
	} else if (do_step) {
		ctrl.dispatches_per_batch = 1;
	}

	if (ctrl.do_reset)
		memset(AER_history, 0, sizeof(AER_history));
	if (run)
		AER_history[(AER_history_idx++)%ARRSIZE(AER_history)] = AER;

	ctrl.stop_at_n_dispatches = gui_set.enable_break_at_step ? gui_set.break_at_step_number : 0;

	if (gui::Begin("Performance")) {
		float sec_per_dispatch = ctrl.dispatches_per_batch == 0 ? 0.0f : sec_per_batch / ctrl.dispatches_per_batch;
		float events_per_sim_sec = stats.event_count_this_batch / sec_per_batch;
		float events_per_wall_sec = sec_per_frame == 0.0f ? 0.0f : stats.event_count_this_batch / sec_per_frame;
		float event_count_this_dispatch = stats.event_count_this_batch / (ctrl.dispatches_per_batch == 0 ? 1 : ctrl.dispatches_per_batch);
		float AER = events_per_sim_sec / total_sites;
		float WAER = events_per_wall_sec / total_sites;
		gui::Text("Events per second:  %3.3fM",    events_per_sim_sec / (1000000.0f)); 
		gui::Text("Batch:              %3.3f ms",  sec_per_batch * 1000.0f);
		gui::Text("Dispatch:           %3.3f ms",  sec_per_dispatch * 1000.0f);
		gui::Text("AER:                %3.3f",     AER);
		gui::Text("WAER:               %3.3f (%.1f%%)", WAER, WAER/(AER == 0.0f ? 1.0f : AER)*100.0f);
		gui::Text("Sites updated:      %.0f (%3.3f%%)", event_count_this_dispatch, event_count_this_dispatch / float(total_sites) * 100.0f);
		gui::Text("Time:               %3.1f sec", sim_time_since_reset);
	} gui::End(); 

	ivec2 prev_world_size = world.size;
	world.resize(gui_world_res, computeGetDescriptorSet(), renderGetDescriptorSet(), renderGetSampler());
	bool world_has_changed = world.size != prev_world_size;
	initStatsIfNeeded();
	bool file_change = false;
	bool project_change = false;
	
	checkForSplatProgramChanges(&file_change, &project_change, &prog_info);
	ctrl.do_reset |= project_change;
	if (!dont_reset_when_code_changes)
		ctrl.do_reset |= file_change;

	if (world_has_changed) { 
		camera_from_world = camera_from_world_start = camera_from_world_target = calcOverviewPose(gui_world_res);
		scale_before_zoomout = scaleof(camera_from_world);
		overview_state = OVERVIEW_INACTIVE;
		ctrl.do_reset = true;
	}

	/* GL UPDATE WAS HERE */

	useProgram("shaders/staged_update_direct.comp", &prog_stats);
	useProgram("shaders/draw.vert", "shaders/draw.frag");
	if (site_info.event_ocurred_signal != 0) {
		log("Break!\n");
		run = false;
		zeroControlSignals();
	}

	open_shader_gui |= checkForShaderErrors();

	/* GL DRAW WAS HERE */

	showSplatCompilerErrors(&prog_info, StringRange(prog_stats.comp_log, prog_stats.comp_log ? strlen(prog_stats.comp_log) : 0), prog_stats.time_to_compile + prog_stats.time_to_link);

	want_stats = false;
	if (gui::Begin("Statistics")) {
		/* #PORT
		gui::AlignFirstTextHeightToWidgets();
		*/
		gui::Text("Atom Counts:");
		gui::SameLine();
		gui::Checkbox("(show zero counts)", &show_zero_counts);
		gui::Separator();
		int max_count_chars = 0;
		int n = total_sites;
		while (n > 0) { max_count_chars += 1; n /= 10; }
		for (int i = 0; i < prog_info.elems.count; ++i) {
			ElementInfo& einfo = prog_info.elems[i];
			bool is_void = einfo.name == StringRange("Void");
			bool show = is_void && stats.counts[i] > 0; // show if there are any Void's
			show |= stats.counts[i] > 0;                // show if the count is nonzero
			show |= !is_void && show_zero_counts;       // show everyone if user asks (unless it's Void)
			if (show) { // show all non-void, and also void if it happens to be non-zero
				bool hover = false;
				gui::PushID(i);
				gui::ColorPip("##color", einfo.color); gui::SameLine();
				hover |= gui::IsItemHovered();
				float percent = float(stats.counts[i]) / float(total_sites);
				if (is_void) gui::PushStyleColor(ImGuiCol_Text, COLOR_ERROR);
				gui::Text("[%-2.*s]: %*d (%7.3f%%)", einfo.symbol.len, einfo.symbol.str, max_count_chars, stats.counts[i], percent * 100.0f);
				hover |= gui::IsItemHovered();
				if (is_void) gui::PopStyleColor();
				gui::SameLine();
				gui::ColorBar("##bar", einfo.color, percent);
				hover |= gui::IsItemHovered();
				if (hover)
				gui::PopID();
			}
		}
		want_stats = true;
	} gui::End();

	if (gui::Begin("Site Inspector")) {
		gui::RadioButton("basic", &inspect_mode, INSPECT_MODE_BASIC); gui::SameLine();
		gui::RadioButton("full", &inspect_mode, INSPECT_MODE_FULL);
		gui::Spacing();
		if (site_info_idx.x >= 0 && site_info_idx.y >= 0 && site_info_idx.x < world.size.x && site_info_idx.y < world.size.y) {
			gui::Text("Site: (%d, %d)", site_info_idx.x, site_info_idx.y);
			if (inspect_mode == INSPECT_MODE_FULL)
				gui::Text("dev: %d %d %d %d", site_info.dev.x, site_info.dev.y, site_info.dev.z, site_info.dev.w);
			guiAtomInspector(site_info.event_layer);
		} else {
			gui::Text("Site: Out of bounds.");
		}
	} gui::End();

	ctimer_stop(); // stats

	if (open_shader_gui)
		guiShader(&open_shader_gui);

	ctimer_stop(); // update
}

void mfmCompute(VkCommandBuffer cb) {
	ctimer_start("compute"); 
	
	ComputeArgs args;
	args.site_info_idx = site_info_idx;
	computeBegin(cb, args);

	if (ctrl.do_reset) {
		ctrl.dispatch_counter = 0;
		events_since_reset = 0;
		sim_time_since_reset = 0.0;
		wall_time_since_reset = 0.0;
		computeStage(cb, STAGE_RESET, world.voteMapSize());
		time_of_reset = time_counter();
		ctrl.do_reset = false;
	}

	mfmClearStats();

	evkTimeQuery();
	if (ctrl.dispatches_per_batch > 0) {;
		//update_timer.reset();
		//update_timer.start("batch");
		//gtimer_start("update");

		for (int i = 0; i < ctrl.dispatches_per_batch; ++i) { 
			if (ctrl.stop_at_n_dispatches != 0 && ctrl.dispatch_counter == ctrl.stop_at_n_dispatches) break;

			gtimer_start("vote");
			computeStage(cb, STAGE_VOTE, world.voteMapSize());
			gtimer_stop();
		
			gtimer_start("tick");
			computeStage(cb, STAGE_EVENT, world.size);
			gtimer_stop();

			ctrl.dispatch_counter++;
		}

		//update_timer.stop();
		//gtimer_stop();
	}
	evkTimeQuery();
	
	//if (want_stats)
	//	mfmComputeStats(gui_world_res);

	//mfmSiteInfo(gui_world_res);
	
	evkTimeQuery();
	computeStage(cb, STAGE_RENDER, world.size);
	evkTimeQuery();

	mfmReadStats();

	ctimer_stop();
}
void mfmRender(VkCommandBuffer cb) {
	ctimer_start("render");
	RenderVis vis;
	vis.event_window_amt = event_window_vis;
	renderDraw(cb, world.size, camera_from_world, vis);
	ctimer_stop();
}