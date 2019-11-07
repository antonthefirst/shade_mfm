#pragma once

struct GLFWwindow;

struct AppInit
{
	int res_x = 0;
	int res_y = 0;
	bool fullscreen = false;
	int antialiasing = 0;
	bool v_sync = true;
	float pos_x = 0.0f; //expresed as % of primary monitor's resolution
	float pos_y = 0.0f;
	const char* title = "nest";
};

struct AppState
{
	GLFWwindow* window;
	int res_x = 0;
	int res_y = 0;
	int refresh_rate = 0;
};

int appInit(AppInit init);
GLFWwindow* appGetWindow();
bool appShouldClose();
void appSwapBuffers();
void appTerm();