#pragma once

#include <string> // for pressed chars

#define PAD_TYPE_PS4  0
#define PAD_TYPE_XBOX 1

#define KEY_SPACE              32
#define KEY_APOSTROPHE         39  /* ' */
#define KEY_COMMA              44  /* , */
#define KEY_MINUS              45  /* - */
#define KEY_PERIOD             46  /* . */
#define KEY_SLASH              47  /* / */
#define KEY_0                  48
#define KEY_1                  49
#define KEY_2                  50
#define KEY_3                  51
#define KEY_4                  52
#define KEY_5                  53
#define KEY_6                  54
#define KEY_7                  55
#define KEY_8                  56
#define KEY_9                  57
#define KEY_SEMICOLON          59  /* ; */
#define KEY_EQUAL              61  /* = */
#define KEY_A                  65
#define KEY_B                  66
#define KEY_C                  67
#define KEY_D                  68
#define KEY_E                  69
#define KEY_F                  70
#define KEY_G                  71
#define KEY_H                  72
#define KEY_I                  73
#define KEY_J                  74
#define KEY_K                  75
#define KEY_L                  76
#define KEY_M                  77
#define KEY_N                  78
#define KEY_O                  79
#define KEY_P                  80
#define KEY_Q                  81
#define KEY_R                  82
#define KEY_S                  83
#define KEY_T                  84
#define KEY_U                  85
#define KEY_V                  86
#define KEY_W                  87
#define KEY_X                  88
#define KEY_Y                  89
#define KEY_Z                  90
#define KEY_LEFT_BRACKET       91  /* [ */
#define KEY_BACKSLASH          92  /* \ */
#define KEY_RIGHT_BRACKET      93  /* ] */
#define KEY_GRAVE_ACCENT       96  /* ` */
#define KEY_WORLD_1            161 /* non-US #1 */
#define KEY_WORLD_2            162 /* non-US #2 */

/* Function keys */
#define KEY_ESCAPE             256
#define KEY_ENTER              257
#define KEY_TAB                258
#define KEY_BACKSPACE          259
#define KEY_INSERT             260
#define KEY_DELETE             261
#define KEY_RIGHT              262
#define KEY_LEFT               263
#define KEY_DOWN               264
#define KEY_UP                 265
#define KEY_PAGE_UP            266
#define KEY_PAGE_DOWN          267
#define KEY_HOME               268
#define KEY_END                269
#define KEY_CAPS_LOCK          280
#define KEY_SCROLL_LOCK        281
#define KEY_NUM_LOCK           282
#define KEY_PRINT_SCREEN       283
#define KEY_PAUSE              284
#define KEY_F1                 290
#define KEY_F2                 291
#define KEY_F3                 292
#define KEY_F4                 293
#define KEY_F5                 294
#define KEY_F6                 295
#define KEY_F7                 296
#define KEY_F8                 297
#define KEY_F9                 298
#define KEY_F10                299
#define KEY_F11                300
#define KEY_F12                301
#define KEY_F13                302
#define KEY_F14                303
#define KEY_F15                304
#define KEY_F16                305
#define KEY_F17                306
#define KEY_F18                307
#define KEY_F19                308
#define KEY_F20                309
#define KEY_F21                310
#define KEY_F22                311
#define KEY_F23                312
#define KEY_F24                313
#define KEY_F25                314
#define KEY_KP_0               320
#define KEY_KP_1               321
#define KEY_KP_2               322
#define KEY_KP_3               323
#define KEY_KP_4               324
#define KEY_KP_5               325
#define KEY_KP_6               326
#define KEY_KP_7               327
#define KEY_KP_8               328
#define KEY_KP_9               329
#define KEY_KP_DECIMAL         330
#define KEY_KP_DIVIDE          331
#define KEY_KP_MULTIPLY        332
#define KEY_KP_SUBTRACT        333
#define KEY_KP_ADD             334
#define KEY_KP_ENTER           335
#define KEY_KP_EQUAL           336
#define KEY_LEFT_SHIFT         340
#define KEY_LEFT_CONTROL       341
#define KEY_LEFT_ALT           342
#define KEY_LEFT_SUPER         343
#define KEY_RIGHT_SHIFT        344
#define KEY_RIGHT_CONTROL      345
#define KEY_RIGHT_ALT          346
#define KEY_RIGHT_SUPER        347
#define KEY_MENU               348
#define KEY_LAST               KEY_MENU

/* Mouse button definitions */
#define MOUSE_BUTTON_1      0
#define MOUSE_BUTTON_2      1
#define MOUSE_BUTTON_3      2
#define MOUSE_BUTTON_4      3
#define MOUSE_BUTTON_5      4
#define MOUSE_BUTTON_6      5
#define MOUSE_BUTTON_7      6
#define MOUSE_BUTTON_8      7
#define MOUSE_BUTTON_LAST   MOUSE_BUTTON_8

/* Mouse button aliases */
#define MOUSE_BUTTON_LEFT   MOUSE_BUTTON_1
#define MOUSE_BUTTON_RIGHT  MOUSE_BUTTON_2
#define MOUSE_BUTTON_MIDDLE MOUSE_BUTTON_3

/* Gamepad button definitions */
#define PAD_MAX      8
#define PAD_CROSS    0
#define PAD_CIRCLE   1
#define PAD_SQUARE   2
#define PAD_TRIANGLE 3
#define PAD_L1       4
#define PAD_L2       5
#define PAD_R1       6
#define PAD_R2       7
#define PAD_LEFT     8
#define PAD_RIGHT    9
#define PAD_UP       10
#define PAD_DOWN     11
//simulated presses from joystick
#define PAD_JOY_LEFT  12
#define PAD_JOY_RIGHT 13
#define PAD_JOY_UP    14
#define PAD_JOY_DOWN  15
//digital only
#define PAD_L3       16
#define PAD_R3       17
#define PAD_START    18
#define PAD_SELECT   19
#define PAD_LAST     20

struct Input {
	struct Key {
		bool press[KEY_LAST + 1];
		bool release[KEY_LAST + 1];
		bool down[KEY_LAST + 1];
	} key;
	struct Mouse {
		float x, y;
		float dx, dy;
		bool valid; //used to signal to subroutines if they have mouse in focus
		bool press[MOUSE_BUTTON_LAST + 1];
		bool release[MOUSE_BUTTON_LAST + 1];
		bool down[MOUSE_BUTTON_LAST + 1];
		float wheel; //delta of wheel movement
	} mouse;
	struct Pad {
		bool press[PAD_LAST + 1];
		bool release[PAD_LAST + 1];
		bool down[PAD_LAST + 1];
		struct {
			float x, y;
		} left_joy;
		struct {
			float x, y;
		} right_joy;
	} pad[PAD_MAX];
	int pad_count;

	std::string chars; //characters inputted during this frame

	bool exit; //signals the user has clicked the [X] of the window

	Input();
};

struct GLFWwindow;

void inputPoll(GLFWwindow* gWindow, Input& in);
