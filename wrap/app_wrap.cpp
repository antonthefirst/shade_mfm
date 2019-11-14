#include "app_wrap.h"
#include "core/log.h"
#include "evk.h"
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <string.h>
#include "core/vec2.h"

#define SYS_GLFW "GLFW"
#define GL_INFO_FILENAME "gl_info.txt"

static GLFWwindow* gWindow;
static bool rebuild_swapchain = false;
static ivec2 new_swapchain_res = ivec2(0);

static void glfw_resize_callback(GLFWwindow*, int w, int h) {
	evkNotifyOfWindow(ivec2(w,h));
}

static void glfw_error_callback(int error, const char* description) {
	logError(SYS_GLFW, error, description);
}

int appInit(AppInit init)
{
	
	// Setup window
	glfwSetErrorCallback(glfw_error_callback);
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

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);

	logInfo(SYS_GLFW, "Creating window...");
	gWindow = glfwCreateWindow(init.res_x, init.res_y, init.title, init.fullscreen ? glfwGetPrimaryMonitor() : NULL, NULL);

	if(!gWindow)
		return 1;

	glfwSetWindowPos(gWindow, int(mode->width*init.pos_x), int(mode->height*init.pos_y));

    // Setup Vulkan
    if (!glfwVulkanSupported()) {
        logError(SYS_GLFW, 0, "Vulkan not supported, exiting.");
        return 1;
    }
	uint32_t extensions_count = 0;
    const char** extensions = glfwGetRequiredInstanceExtensions(&extensions_count);
    evkInit(extensions, extensions_count);

    // Create Window Surface
    VkSurfaceKHR surface;
    VkResult err = glfwCreateWindowSurface(evk.inst, gWindow, evk.alloc, &surface);
    evkCheckError(err);

	
    // Create Framebuffers
    int w, h;
    glfwGetFramebufferSize(gWindow, &w, &h);
    glfwSetFramebufferSizeCallback(gWindow, glfw_resize_callback);
	evkSelectSurfaceFormatAndPresentMode(surface);
	evkResizeWindow(ivec2(w,h), mode->refreshRate);

	return 0;
}

GLFWwindow* appGetWindow() {
	return gWindow;
}

bool appShouldClose() {
	return (glfwWindowShouldClose(gWindow) != 0) || glfwGetKey(gWindow, GLFW_KEY_ESCAPE);
}

void appSwapBuffers()
{
	glfwSwapBuffers(gWindow);
}
void appWaitForEvents() {
	glfwWaitEvents();
}
void appTerm()
{
	glfwTerminate();
	evkTerm();
}



















#if __APPLE__
glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
