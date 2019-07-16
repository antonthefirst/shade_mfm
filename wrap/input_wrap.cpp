#include "input_wrap.h"
#include "app_wrap.h"
#include "imgui/imgui.h"
#include "imgui_impl_glfw_gl3.h" // to pass it the scroll callback
#include <GLFW/glfw3.h>
#include <string.h> // for memset
#include <core/maths.h>
#include <core/log.h>
#include <string>

extern GLFWwindow* gWindow;

static bool first_frame = true;

// key state
static char keyPrev[KEY_LAST + 1];
std::string pressedChars;

// mouse state
static int wheelPrev = 0;
static float mouseXPrev = 0;
static float mouseYPrev = 0;
static char mousePrev[MOUSE_BUTTON_LAST + 1];
static double wheelScrollSum = 0.0;

// pad state
static char padPrev[PAD_MAX][PAD_LAST + 1];
static int padType[PAD_MAX];


void scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
	wheelScrollSum += yoffset;
	ImGui_ImplGlfw_ScrollCallback(window, xoffset, yoffset);
}

Input::Input() {
	memset(&key, 0, sizeof(key));
	memset(&mouse, 0, sizeof(mouse));
	memset(&pad, 0, sizeof(pad));
	pad_count = 0;
	exit = false;
}

#if 0
static float applyDeadzone(float v, float joy_deadzone) {
	const float range = 1.f - joy_deadzone;
	if (fabs(v) < joy_deadzone) {
		if (v > 0.f)
			return max(0.f, v - joy_deadzone);
		else
			return min(0.f, v + joy_deadzone);
	} else {
		if (v > 0.f)
			return (v - joy_deadzone) / range;
		else
			return (v + joy_deadzone) / range;
	}
}
#endif


void inputPoll(Input& in)
{
	if (first_frame) {
		glfwSetScrollCallback(gWindow, scrollCallback);
		first_frame = false;
	}
	glfwPollEvents();
	AppState s;
	appGetState(s);

	//update the "previous"
	memcpy(keyPrev, in.key.down, sizeof(keyPrev));
	memcpy(mousePrev, in.mouse.down, sizeof(mousePrev));
	for (int i = 0; i < PAD_MAX; ++i)
		memcpy(padPrev[i], in.pad[i].down, sizeof(padPrev[i]));

	//update the current
	for (int i = 32; i <= KEY_LAST; ++i) {
		in.key.down[i] = glfwGetKey(gWindow, i) ? true : false;
		in.key.press[i] = in.key.down[i] && !keyPrev[i];
		in.key.release[i] = !in.key.down[i] && keyPrev[i];
	}

	double fmx, fmy;
	glfwGetCursorPos(gWindow, &fmx, &fmy);
	int mx = int(floor(fmx));
	int my = int(floor(fmy));
	my = int(s.res_y - my);
	in.mouse.x = float(mx) / float(s.res_x);
	in.mouse.y = float(my) / float(s.res_y);
	in.mouse.dx = in.mouse.x - mouseXPrev;
	in.mouse.dy = in.mouse.y - mouseYPrev;
	in.mouse.valid = true;
	for (int i = 0; i <= MOUSE_BUTTON_LAST; ++i) {
		in.mouse.down[i] = glfwGetMouseButton(gWindow, i) ? true : false;
		in.mouse.press[i] = in.mouse.down[i] & !mousePrev[i];
		in.mouse.release[i] = !in.mouse.down[i] & mousePrev[i];
	}
	in.mouse.wheel = float(wheelScrollSum);
	wheelScrollSum = 0.0;
	mouseXPrev = in.mouse.x;
	mouseYPrev = in.mouse.y;

	in.pad_count = 0;
	//int i = 0;
#if 0
	static int present[PAD_MAX] = { -1,-1,-1,-1,-1,-1,-1,-1 };
	for (int j = 0; j < PAD_MAX; ++j) {
		memset(in.pad[i].down, 0, sizeof(in.pad[i].down));
		if (present[j] == -1) {
			present[j] = glfwGetJoystickParam(j, GLFW_PRESENT);
		}
		// if (glfwGetJoystickParam(j, GLFW_PRESENT)) {
		if (present[j] > 0) {
			float joy[8];
			glfwGetJoystickPos(j, joy, 8);

			in.pad[i].left_joy.x = applyDeadzone(joy[0], joy_deadzone);
			in.pad[i].left_joy.y = applyDeadzone(joy[1], joy_deadzone);
			in.pad[i].right_joy.x = applyDeadzone(joy[4], joy_deadzone);
			in.pad[i].right_joy.y = applyDeadzone(-joy[3], joy_deadzone);

			//printf("%d axes %d buttons\n", glfwGetJoystickParam(j, GLFW_AXES), glfwGetJoystickParam(j, GLFW_BUTTONS));
			//printf("%f %f %f\n", joy[2], joy[5], joy[6]);

			int num_buttons = glfwGetJoystickParam(j, GLFW_BUTTONS);
			unsigned char buttons[64];
			memset(buttons, 0, sizeof(buttons));
			glfwGetJoystickButtons(j, buttons, num_buttons);
			in.pad[i].down[PAD_CROSS] = buttons[0] ? true : false;
			in.pad[i].down[PAD_CIRCLE] = buttons[1] ? true : false;
			in.pad[i].down[PAD_SQUARE] = buttons[2] ? true : false;
			in.pad[i].down[PAD_TRIANGLE] = buttons[3] ? true : false;
			in.pad[i].down[PAD_L1] = buttons[4] ? true : false;
			in.pad[i].down[PAD_R1] = buttons[5] ? true : false;
			in.pad[i].down[PAD_SELECT] = buttons[6] ? true : false;
			in.pad[i].down[PAD_START] = buttons[7] ? true : false;
			in.pad[i].down[PAD_L3] = buttons[8] ? true : false;
			in.pad[i].down[PAD_R3] = buttons[9] ? true : false;

			//since there is no hat support :(
			in.pad[i].down[PAD_LEFT] = in.pad[i].down[PAD_SQUARE];
			in.pad[i].down[PAD_RIGHT] = in.pad[i].down[PAD_CIRCLE];
			in.pad[i].down[PAD_UP] = in.pad[i].down[PAD_TRIANGLE];
			in.pad[i].down[PAD_DOWN] = in.pad[i].down[PAD_CROSS];

			for (int b = 0; b < PAD_LAST; ++b) {
				in.pad[i].press[b] = in.pad[i].down[b] & !padPrev[i][b];
				in.pad[i].release[b] = !in.pad[i].down[b] & padPrev[i][b];
			}
			//for (int i = 0; i < 64; ++i)
			//	if (buttons[i])
			//		printf("Button %d\n", i);
			++i;
			++in.pad_count;
		}
		/*else {
		memset(in.pad[i].press, 0, sizeof(in.pad[i].press));
		memset(in.pad[i].release, 0, sizeof(in.pad[i].release));
		}*/
	}
#endif
	in.chars = pressedChars;
	pressedChars.clear();
}
