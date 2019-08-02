#include "wrap/input_wrap.h"
#include "wrap/app_wrap.h"
#include "wrap/imgui_wrap.h"
#include "wrap/recorder_wrap.h"
#include "core/maths.h"
#include <imgui/imgui.h>
#include "core/log.h"
#include "core/cpu_timer.h"
#include "timers.h"
#include "core/shader_loader.h"
#include "core/gl_message_log.h"

void mfmInit(float aspect);
void mfmTerm();
void mfmUpdate(Input* in, int app_res_x, int app_res_y, int refresh_rate);

int main(int, char**)
{
	timeSetStart();

	int ret = 0;

	AppInit init;

	init.v_sync = false;

#define MODE 0
#if MODE==0 // same monitor
	init.res_x = 1920;
	init.res_y = 1080;
	init.pos_x = 0.1f;
	init.pos_y = 0.1f;
	init.fullscreen = false;
#elif MODE==1 // second monitor on the right
	init.res_x = 1920;
	init.res_y = 1080;
	init.pos_x = 1.0f;
	init.fullscreen = false;
#elif MODE==2 // highres fullscreen
	init.res_x = 2560;
	init.res_y = 1440;
	init.fullscreen = true;
#elif MODE==3 // lowres fullscreen
	init.res_x = 1920;
	init.res_y = 1080;
	init.fullscreen = true;
#elif MODE==4 // lowres window
	init.res_x = 1280;
	init.res_y = 720;
	init.pos_x = 0.4f;
	init.pos_y = 0.1f;
	init.fullscreen = false;
#elif MODE==5 // video capture
	init.res_x = 1920;
	init.res_y = 1080;
	init.pos_x = 0.0f;
	init.pos_y = 0.0f;
	init.fullscreen = false;
#endif

	init.title = "shade_mfm";

	ret = appInit(init);
	if (ret) return ret;
	glMessageLogInit();

	Input in;
	AppState app_state;
	appGetState(app_state);

	imguiInit();

    while (!appShouldClose())
    {
		inputPoll(in);
		appGetState(app_state);

		if (in.key.press[KEY_F2])
			recUIToggle();

		imguiNewFrame();
		ctimer_stop();
		ctimer_reset();

		ctimer_start("frame");
		mfmUpdate(&in, app_state.res_x, app_state.res_y, app_state.refresh_rate);

		bool capture_req = in.key.press[KEY_F9];
		if (gui::Begin("Capture")) {
			capture_req |= gui::Button("screenshot");
		} gui::End();

		recUI(capture_req);
		gtimer_stop();
		gtimer_reset();
		timerUI(in);
		glMessageLogUI();
		gtimer_start("frame");
		gtimer_start("gui");
		imguiRender();
		gtimer_stop();

		gtimer_start("record");
		recFrame("all", 0,0,app_state.res_x,app_state.res_y);
		recUpdate();
		gtimer_stop();

		gtimer_start("swap");
		appSwapBuffers();
		gtimer_stop();
	}

	imguiTerm();
	appTerm();

    return 0;
}
