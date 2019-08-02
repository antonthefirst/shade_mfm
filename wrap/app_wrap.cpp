#include "app_wrap.h"
#include "core/log.h"

#include <GL/gl3w.h>
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <string.h>
GLFWwindow* gWindow;
#define SYS_GLFW " GLFW "
#define SYS_GL3W " gl3w "

#define GL_INFO_FILENAME "gl_info.txt"

static void error_callback(int error, const char* description) {
	logError(SYS_GLFW, error, description);
}

int appInit(AppInit init)
{
	// Setup window
	glfwSetErrorCallback(error_callback);
	logInfo(SYS_GLFW, "Initializing...");
	if (!glfwInit())
		return 1;

	logInfo(SYS_GLFW, "Getting Monitor...");
	GLFWmonitor* monitor = glfwGetPrimaryMonitor();
	if (!monitor)
		return 1;
	logInfo(SYS_GLFW, "Checking video mode...");
	const GLFWvidmode* mode = glfwGetVideoMode(monitor);
	if (!mode)
		return 1;

	const int version_major = 4;
	const int version_minor = 3;
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, version_major);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, version_minor);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	if (init.antialiasing != 0) glfwWindowHint(GLFW_SAMPLES, init.antialiasing);
	glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);

	logInfo(SYS_GLFW, "Creating window...");
	gWindow = glfwCreateWindow(init.res_x, init.res_y, init.title, init.fullscreen ? glfwGetPrimaryMonitor() : NULL, NULL);

	if(!gWindow)
		return 1;

	glfwSetWindowPos(gWindow, int(mode->width*init.pos_x), int(mode->height*init.pos_y));

	logInfo(SYS_GLFW, "Setting OpenGL context to window...");
	glfwMakeContextCurrent(gWindow);
	glfwSwapInterval(init.v_sync ? 1 : 0);
	logInfo(SYS_GLFW, "Initialization complete");

	logInfo(SYS_GL3W, "Initializing...");
	if (int ret = gl3wInit()) {
		logError(SYS_GL3W, ret, "Initialization error, could not find version.");
	}
	if (gl3wIsSupported(version_major, version_minor)) {
		logInfo(SYS_GL3W, "Version %d.%d is supported", version_major, version_minor);
	} else {
		logError(SYS_GL3W, 0, "Version %d.%d is not supported", version_major, version_minor);
	}
	logInfo(SYS_GL3W, "Initialization complete");

	/*
	logInfo(SYS_GL, "Checking GL capabilities and writing them to %s", GL_INFO_FILENAME);
	FILE* f = fopen(GL_INFO_FILENAME, "w");
	if (f) {
		fprintf(f, "Vendor:   %s\n", glGetString(GL_VENDOR));
		fprintf(f, "Renderer: %s\n", glGetString(GL_RENDERER));
		fprintf(f, "Version:  %s\n", glGetString(GL_VERSION));
		fprintf(f, "Shader:   %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
		GLint extension_count = 0;
		glGetIntegerv(GL_NUM_EXTENSIONS, &extension_count);
		fprintf(f, "Extensions:\n");
		for (int i = 0; i < extension_count; ++i) {
			fprintf(f, (const char*)glGetStringi(GL_EXTENSIONS,i));
			fprintf(f, "\n");
		}
		fclose(f);
	} else {
		logError(SYS_GL, 0, "Unable to open file for writing info");
	}
	*/

	/* There are no extensions to require right now...
	logInfo(SYS_GL, "Checking required extensions...");
	const char* required_extensions[] = { };
	GLint extension_count = 0;
	glGetIntegerv(GL_NUM_EXTENSIONS, &extension_count);
	bool all_found = true;
	for (int e = 0; e < ARRSIZE(required_extensions); ++e) {
		bool found = false;
		for (int i = 0; i < extension_count; ++i) {
			if (strcmp(required_extensions[e], (const char*)glGetStringi(GL_EXTENSIONS,i)) == 0) {
				logInfo(SYS_GL, "%s - OK", required_extensions[e]);
				found = true;
				break;
			}
		}
		if (!found) {
			logError(SYS_GL, 0, "%s - NOT FOUND", required_extensions[e]);
		}
		all_found &= found;
	}
	if (!all_found) {
		logError(SYS_GL, 0, "Not all extensions are available");
	}
	*/

	return 0;
}

void appGetState(AppState& state)
{
	glfwGetFramebufferSize(gWindow, &state.res_x, &state.res_y);
	GLFWmonitor* monitor = glfwGetPrimaryMonitor();
	const GLFWvidmode* mode = glfwGetVideoMode(monitor);
	state.refresh_rate = mode->refreshRate; //#DODGY we are basically assuming that the primary monitor's windows request succeeded
}

bool appShouldClose()
{
	return (glfwWindowShouldClose(gWindow) != 0) || glfwGetKey(gWindow, GLFW_KEY_ESCAPE);
}

void appSwapBuffers()
{
	glfwSwapBuffers(gWindow);
}

void appTerm()
{
	glfwTerminate();
}



















#if __APPLE__
glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
