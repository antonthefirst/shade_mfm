#include "wrap/input_wrap.h"
#include "wrap/app_wrap.h"
#include "wrap/recorder_wrap.h"
#include "core/maths.h"
#include <imgui/imgui.h>
#include "core/log.h"
#include "core/cpu_timer.h"
#include "core/shader_loader.h"
#include "timers.h"
#include "core/vec2.h"
#include <GLFW/glfw3.h>

#include "wrap/evk.h"
#include "wrap/imgui_impl_glfw.h"
#include "wrap/imgui_impl_vulkan.h"
#include <vulkan/vulkan.h>

static void imguiInit(GLFWwindow* window) {
	// Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();

    // Setup Platform/Renderer bindings
    ImGui_ImplGlfw_InitForVulkan(window, true);
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = evk.inst;
    init_info.PhysicalDevice = evk.phys_dev;
    init_info.Device = evk.dev;
    init_info.QueueFamily = evk.que_fam;
    init_info.Queue = evk.que;
    init_info.PipelineCache = evk.pipe_cache;
    init_info.DescriptorPool = evk.desc_pool;
    init_info.Allocator = evk.alloc;
    init_info.MinImageCount = evkMinImageCount();
    init_info.ImageCount = evk.win.ImageCount;
    init_info.CheckVkResultFn = evkGetCheckErrorFunc();
    ImGui_ImplVulkan_Init(&init_info, evk.win.RenderPass);

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Read 'misc/fonts/README.txt' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    //io.Fonts->AddFontDefault();
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/ProggyTiny.ttf", 10.0f);
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
    //IM_ASSERT(font != NULL);

    // Upload Fonts
    {
        // Use any command queue
        VkCommandPool command_pool = evk.win.Frames[evk.win.FrameIndex].CommandPool;
        VkCommandBuffer command_buffer = evk.win.Frames[evk.win.FrameIndex].CommandBuffer;
		
		VkResult err;
        err = vkResetCommandPool(evk.dev, command_pool, 0);
        evkCheckError(err);
        VkCommandBufferBeginInfo begin_info = {};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        err = vkBeginCommandBuffer(command_buffer, &begin_info);
        evkCheckError(err);

        ImGui_ImplVulkan_CreateFontsTexture(command_buffer);

        VkSubmitInfo end_info = {};
        end_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        end_info.commandBufferCount = 1;
        end_info.pCommandBuffers = &command_buffer;
        err = vkEndCommandBuffer(command_buffer);
        evkCheckError(err);
        err = vkQueueSubmit(evk.que, 1, &end_info, VK_NULL_HANDLE);
        evkCheckError(err);

        err = vkDeviceWaitIdle(evk.dev);
        evkCheckError(err);
        ImGui_ImplVulkan_DestroyFontUploadObjects();
    }
}
static void imguiTerm() {
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}
static void imguiNewFrame() {
	ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

extern void mfmTerm();
extern void mfmUpdate(Input* main_in);
extern void mfmCompute(VkCommandBuffer cb);
extern void mfmRender(VkCommandBuffer cb);

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

	init.title = "evk";

	ret = appInit(init);
	if (ret) return ret;

	Input in;
	imguiInit(appGetWindow());

    while (!appShouldClose())
    {
		inputPoll(appGetWindow(), in);
		if (evkCheckForSwapchainChanges()) {
			ImGui_ImplVulkan_SetMinImageCount(evkMinImageCount());
		}
		if (!evkWindowIsMinimized()) {
			if (in.key.press[KEY_F2])
				recUIToggle();

			imguiNewFrame();

			ctimer_reset();
			ctimer_gui();
			gtimer_gui();

			//ImGui::ShowDemoWindow();
			mfmUpdate(&in);
			guiShader();

			ImGui::Render();

			//21/255.f, 33/255.f, 54/255.f
			ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
			memcpy(&evk.win.ClearValue.color.float32[0], &clear_color, 4 * sizeof(float));

			if (evkFrameAcquire()) {

				gtimer_reset(evkGetRenderCommandBuffer());

				mfmCompute(evkGetRenderCommandBuffer());

				evkRenderBegin();
				mfmRender(evkGetRenderCommandBuffer());
				ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), evkGetRenderCommandBuffer());
		
				evkRenderEnd();

				evkFramePresent();
			}
		} else {
			appWaitForEvents();
		}
	}
	evkWaitUntilReadyToTerm();
	mfmTerm();
	imguiTerm();
	shadersDestroy();
	gtimer_term();
	appTerm();

    return 0;
}
