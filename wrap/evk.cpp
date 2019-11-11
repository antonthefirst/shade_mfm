#include "evk.h"
#include <string.h>   // for memcpy
#include "core/log.h" // for assert
#include "core/file_stat.h"    // for loading binary
#include "core/string_range.h" // for compiler output
#include "core/runprog.h"      // for calling compiler
#include "core/container.h"    // for surface results

#define QUERIES_PER_FRAME_MAX (64)

static int getMinImageCountFromPresentMode(VkPresentModeKHR present_mode);
static void destroyAllFramesAndSemaphoresAndTimestamps();

EasyVk evk;

const char* errorString(VkResult errorCode) {
	switch (errorCode)
	{
#define STR(r) case VK_ ##r: return #r
		STR(NOT_READY);
		STR(TIMEOUT);
		STR(EVENT_SET);
		STR(EVENT_RESET);
		STR(INCOMPLETE);
		STR(ERROR_OUT_OF_HOST_MEMORY);
		STR(ERROR_OUT_OF_DEVICE_MEMORY);
		STR(ERROR_INITIALIZATION_FAILED);
		STR(ERROR_DEVICE_LOST);
		STR(ERROR_MEMORY_MAP_FAILED);
		STR(ERROR_LAYER_NOT_PRESENT);
		STR(ERROR_EXTENSION_NOT_PRESENT);
		STR(ERROR_FEATURE_NOT_PRESENT);
		STR(ERROR_INCOMPATIBLE_DRIVER);
		STR(ERROR_TOO_MANY_OBJECTS);
		STR(ERROR_FORMAT_NOT_SUPPORTED);
		STR(ERROR_SURFACE_LOST_KHR);
		STR(ERROR_NATIVE_WINDOW_IN_USE_KHR);
		STR(SUBOPTIMAL_KHR);
		STR(ERROR_OUT_OF_DATE_KHR);
		STR(ERROR_INCOMPATIBLE_DISPLAY_KHR);
		STR(ERROR_VALIDATION_FAILED_EXT);
		STR(ERROR_INVALID_SHADER_NV);
#undef STR
	default:
		return "UNKNOWN_ERROR";
	}
}

static void check_vk_result(VkResult err) {
    if (err == 0) return;
	logError("VK", err, errorString(err));
    if (err < 0)
        abort();
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_report(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType, uint64_t object, size_t location, int32_t messageCode, const char* pLayerPrefix, const char* pMessage, void* pUserData) {
    (void)flags; (void)object; (void)location; (void)messageCode; (void)pUserData; (void)pLayerPrefix; // Unused arguments
	logError("VK", objectType, pMessage);
    return VK_FALSE;
}


EasyVkWindow::EasyVkWindow() {
	memset(this, 0, sizeof(*this));
	PresentMode = VK_PRESENT_MODE_MAX_ENUM_KHR;
	ClearEnable = true;
}

void evkInit(const char** extensions, uint32_t extensions_count) {
	evk.alloc = NULL;
	evk.inst = VK_NULL_HANDLE;
	evk.phys_dev = VK_NULL_HANDLE;
	evk.dev = VK_NULL_HANDLE;
	evk.que_fam = (uint32_t)-1;
	evk.que = VK_NULL_HANDLE;
	evk.pipe_cache = VK_NULL_HANDLE;
	evk.desc_pool = VK_NULL_HANDLE;
	evk.debug = VK_NULL_HANDLE;

	VkResult err;

    // Create Vulkan Instance
    {
        VkInstanceCreateInfo create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        create_info.enabledExtensionCount = extensions_count;
        create_info.ppEnabledExtensionNames = extensions;

		// Debug Features
		{
			// Enabling multiple validation layers grouped as LunarG standard validation
			const char* layers[] = { "VK_LAYER_LUNARG_standard_validation" };
			create_info.enabledLayerCount = 1;
			create_info.ppEnabledLayerNames = layers;

			// Enable debug report extension (we need additional storage, so we duplicate the user array to add our new extension to it)
			const char** extensions_ext = (const char**)malloc(sizeof(const char*) * (extensions_count + 1));
			memcpy(extensions_ext, extensions, extensions_count * sizeof(const char*));
			extensions_ext[extensions_count] = "VK_EXT_debug_report";
			create_info.enabledExtensionCount = extensions_count+1;
			create_info.ppEnabledExtensionNames = extensions_ext;

			// Create Vulkan Instance
			err = vkCreateInstance(&create_info, evk.alloc, &evk.inst);
			check_vk_result(err);
			free(extensions_ext);

			// Get the function pointer (required for any extensions)
			auto vkCreateDebugReportCallbackEXT = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(evk.inst, "vkCreateDebugReportCallbackEXT");
			assert(vkCreateDebugReportCallbackEXT != NULL);

			// Setup the debug report callback
			VkDebugReportCallbackCreateInfoEXT debug_report_ci = {};
			debug_report_ci.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
			debug_report_ci.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
			debug_report_ci.pfnCallback = debug_report;
			debug_report_ci.pUserData = NULL;
			err = vkCreateDebugReportCallbackEXT(evk.inst, &debug_report_ci, evk.alloc, &evk.debug);
			check_vk_result(err);
		}
    }

    // Select GPU
    {
        uint32_t gpu_count;
        err = vkEnumeratePhysicalDevices(evk.inst, &gpu_count, NULL);
        check_vk_result(err);
        assert(gpu_count > 0);

        VkPhysicalDevice* gpus = (VkPhysicalDevice*)malloc(sizeof(VkPhysicalDevice) * gpu_count);
        err = vkEnumeratePhysicalDevices(evk.inst, &gpu_count, gpus);
        check_vk_result(err);

        // If a number >1 of GPUs got reported, you should find the best fit GPU for your purpose
        // e.g. VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU if available, or with the greatest memory available, etc.
        // for sake of simplicity we'll just take the first one, assuming it has a graphics queue family.
        evk.phys_dev = gpus[0];
        free(gpus);
    }

	// Get stats about physical device
	{
		vkGetPhysicalDeviceProperties(evk.phys_dev, &evk.phys_props);
	}

    // Select graphics queue family
    {
        uint32_t count;
        vkGetPhysicalDeviceQueueFamilyProperties(evk.phys_dev, &count, NULL);
        VkQueueFamilyProperties* queues = (VkQueueFamilyProperties*)malloc(sizeof(VkQueueFamilyProperties) * count);
        vkGetPhysicalDeviceQueueFamilyProperties(evk.phys_dev, &count, queues);
        for (uint32_t i = 0; i < count; i++)
            if (queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                evk.que_fam = i;
                break;
            }
		evk.que_fam_props = queues[evk.que_fam];
        free(queues);
        assert(evk.que_fam != (uint32_t)-1);
    }

    // Create Logical Device (with 1 queue)
    {
        int device_extension_count = 1;
        const char* device_extensions[] = { "VK_KHR_swapchain" };
        const float queue_priority[] = { 1.0f };
        VkDeviceQueueCreateInfo queue_info[1] = {};
        queue_info[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_info[0].queueFamilyIndex = evk.que_fam;
        queue_info[0].queueCount = 1;
        queue_info[0].pQueuePriorities = queue_priority;
        VkDeviceCreateInfo create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        create_info.queueCreateInfoCount = sizeof(queue_info) / sizeof(queue_info[0]);
        create_info.pQueueCreateInfos = queue_info;
        create_info.enabledExtensionCount = device_extension_count;
        create_info.ppEnabledExtensionNames = device_extensions;
        err = vkCreateDevice(evk.phys_dev, &create_info, evk.alloc, &evk.dev);
        check_vk_result(err);
        vkGetDeviceQueue(evk.dev, evk.que_fam, 0, &evk.que);
    }

    // Create Descriptor Pool
    {
        VkDescriptorPoolSize pool_sizes[] =
        {
            { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
        };
        VkDescriptorPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_info.maxSets = 1000 * ARRSIZE(pool_sizes);
        pool_info.poolSizeCount = (uint32_t)ARRSIZE(pool_sizes);
        pool_info.pPoolSizes = pool_sizes;
        err = vkCreateDescriptorPool(evk.dev, &pool_info, evk.alloc, &evk.desc_pool);
        check_vk_result(err);
    }
}
void evkTerm() {
	destroyAllFramesAndSemaphoresAndTimestamps();
    vkDestroyRenderPass(evk.dev, evk.win.RenderPass, evk.alloc);
    vkDestroySwapchainKHR(evk.dev, evk.win.Swapchain, evk.alloc);
    vkDestroySurfaceKHR(evk.inst, evk.win.Surface, evk.alloc);

    evk.win = EasyVkWindow();

	// Destroy core vulkan objects
	vkDestroyDescriptorPool(evk.dev, evk.desc_pool, evk.alloc);
    auto vkDestroyDebugReportCallbackEXT = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(evk.inst, "vkDestroyDebugReportCallbackEXT");
    vkDestroyDebugReportCallbackEXT(evk.inst, evk.debug, evk.alloc);
    vkDestroyDevice(evk.dev, evk.alloc);
    vkDestroyInstance(evk.inst, evk.alloc);
}

VkShaderModule evkCreateShaderFromFile(const char* pathfile) {
	VkResult err;
	String output;

	runProg(TempStr("glslc -o %s.spv %s", pathfile, pathfile), &output);

	if (output.len > 0) {
		log(output.str);
		return VK_NULL_HANDLE; //#TODO right now this will bail on warnings too
	}
	output.free();

	VkShaderModule module = VK_NULL_HANDLE;
    VkShaderModuleCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	info.pCode = (uint32_t*)fileReadBinaryIntoMem(TempStr("%s.spv", pathfile), &info.codeSize);
    err = vkCreateShaderModule(evk.dev, &info, evk.alloc, &module);
	check_vk_result(err);
	free((void*)info.pCode);

	return module;
}
uint32_t evkMemoryType(VkMemoryPropertyFlags properties, uint32_t type_bits) {
    VkPhysicalDeviceMemoryProperties prop;
    vkGetPhysicalDeviceMemoryProperties(evk.phys_dev, &prop);
    for (uint32_t i = 0; i < prop.memoryTypeCount; i++)
        if ((prop.memoryTypes[i].propertyFlags & properties) == properties && type_bits & (1<<i))
            return i;
    return 0xFFFFFFFF; // Unable to find memoryType
}
int evkMinImageCount() {
	return getMinImageCountFromPresentMode(evk.win.PresentMode);
}
static VkSurfaceFormatKHR SelectSurfaceFormat(VkPhysicalDevice physical_device, VkSurfaceKHR surface, const VkFormat* request_formats, int request_formats_count, VkColorSpaceKHR request_color_space) {
	assert(request_formats != NULL);
	assert(request_formats_count > 0);

	// Per Spec Format and View Format are expected to be the same unless VK_IMAGE_CREATE_MUTABLE_BIT was set at image creation
	// Assuming that the default behavior is without setting this bit, there is no need for separate Swapchain image and image view format
	// Additionally several new color spaces were introduced with Vulkan Spec v1.0.40,
	// hence we must make sure that a format with the mostly available color space, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR, is found and used.
	uint32_t avail_count;
	vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &avail_count, NULL);
	Bunch<VkSurfaceFormatKHR> avail_format;
	avail_format.setgarbage((int)avail_count);
	vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &avail_count, avail_format.ptr);

	// First check if only one format, VK_FORMAT_UNDEFINED, is available, which would imply that any format is available
	if (avail_count == 1)
	{
		if (avail_format[0].format == VK_FORMAT_UNDEFINED)
		{
			VkSurfaceFormatKHR ret;
			ret.format = request_formats[0];
			ret.colorSpace = request_color_space;
			return ret;
		}
		else
		{
			// No point in searching another format
			return avail_format[0];
		}
	}
	else
	{
		// Request several formats, the first found will be used
		for (int request_i = 0; request_i < request_formats_count; request_i++)
			for (uint32_t avail_i = 0; avail_i < avail_count; avail_i++)
				if (avail_format[avail_i].format == request_formats[request_i] && avail_format[avail_i].colorSpace == request_color_space)
					return avail_format[avail_i];

		// If none of the requested image formats could be found, use the first available
		return avail_format[0];
	}
}

static VkPresentModeKHR SelectPresentMode(VkPhysicalDevice physical_device, VkSurfaceKHR surface, const VkPresentModeKHR* request_modes, int request_modes_count){
	assert(request_modes != NULL);
	assert(request_modes_count > 0);

	// Request a certain mode and confirm that it is available. If not use VK_PRESENT_MODE_FIFO_KHR which is mandatory
	uint32_t avail_count = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &avail_count, NULL);
	Bunch<VkPresentModeKHR> avail_modes;
	avail_modes.setgarbage((int)avail_count);
	vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &avail_count, avail_modes.ptr);
	//for (uint32_t avail_i = 0; avail_i < avail_count; avail_i++)
	//    printf("[vulkan] avail_modes[%d] = %d\n", avail_i, avail_modes[avail_i]);

	for (int request_i = 0; request_i < request_modes_count; request_i++)
		for (uint32_t avail_i = 0; avail_i < avail_count; avail_i++)
			if (request_modes[request_i] == avail_modes[avail_i])
				return request_modes[request_i];

	return VK_PRESENT_MODE_FIFO_KHR; // Always available
}
void evkSelectSurfaceFormatAndPresentMode(VkSurfaceKHR surface) {
	evk.win.Surface = surface;

	// Check for WSI support
	VkBool32 res;
	vkGetPhysicalDeviceSurfaceSupportKHR(evk.phys_dev, evk.que_fam, evk.win.Surface, &res);
	if (res != VK_TRUE) {
		logError("VK", 0, "Error no WSI support on physical device 0, exiting.");
		exit(-1);
	}

	// Select Surface Format
	const VkFormat requestSurfaceImageFormat[] = { VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM };
	const VkColorSpaceKHR requestSurfaceColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
	evk.win.SurfaceFormat = SelectSurfaceFormat(evk.phys_dev, evk.win.Surface, requestSurfaceImageFormat, (size_t)ARRSIZE(requestSurfaceImageFormat), requestSurfaceColorSpace);

	// Select Present Mode
	#ifdef UNLIMITED_FRAME_RATE
	VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR, VK_PRESENT_MODE_FIFO_KHR };
	#else
	VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_FIFO_KHR };
	#endif
	evk.win.PresentMode = SelectPresentMode(evk.phys_dev, evk.win.Surface, &present_modes[0], ARRSIZE(present_modes));
	log("PICKED MODE %d\n", evk.win.PresentMode);
}
void evkResizeWindow(ivec2 res) {
    VkResult err;
    VkSwapchainKHR old_swapchain = evk.win.Swapchain;
    err = vkDeviceWaitIdle(evk.dev);
    check_vk_result(err);

    // Destroy old Framebuffer
    // We don't use DestroyWindow() because we want to preserve the old swapchain to create the new one.
	destroyAllFramesAndSemaphoresAndTimestamps();
    if (evk.win.RenderPass)
        vkDestroyRenderPass(evk.dev, evk.win.RenderPass, evk.alloc);

	int min_image_count = evkMinImageCount();

    // Create Swapchain
    {
        VkSwapchainCreateInfoKHR info = {};
        info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        info.surface = evk.win.Surface;
        info.minImageCount = min_image_count;
        info.imageFormat = evk.win.SurfaceFormat.format;
        info.imageColorSpace = evk.win.SurfaceFormat.colorSpace;
        info.imageArrayLayers = 1;
        info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;           // Assume that graphics family == present family
        info.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
        info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        info.presentMode = evk.win.PresentMode;
        info.clipped = VK_TRUE;
        info.oldSwapchain = old_swapchain;
        VkSurfaceCapabilitiesKHR cap;
        err = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(evk.phys_dev, evk.win.Surface, &cap);
        check_vk_result(err);
        if (info.minImageCount < cap.minImageCount)
            info.minImageCount = cap.minImageCount;
        else if (cap.maxImageCount != 0 && info.minImageCount > cap.maxImageCount)
            info.minImageCount = cap.maxImageCount;

        if (cap.currentExtent.width == 0xffffffff)
        {
            info.imageExtent.width = evk.win.Width = res.x;
            info.imageExtent.height = evk.win.Height = res.y;
        }
        else
        {
            info.imageExtent.width = evk.win.Width = cap.currentExtent.width;
            info.imageExtent.height = evk.win.Height = cap.currentExtent.height;
        }
        err = vkCreateSwapchainKHR(evk.dev, &info, evk.alloc, &evk.win.Swapchain);
        check_vk_result(err);
        err = vkGetSwapchainImagesKHR(evk.dev, evk.win.Swapchain, &evk.win.ImageCount, NULL);
        check_vk_result(err);
        VkImage backbuffers[16] = {};
        assert(evk.win.ImageCount >= min_image_count);
        assert(evk.win.ImageCount < ARRSIZE(backbuffers));
        err = vkGetSwapchainImagesKHR(evk.dev, evk.win.Swapchain, &evk.win.ImageCount, backbuffers);
        check_vk_result(err);

		evk.win.FrameTimestampsCount = evk.win.ImageCount * 3; // enough frames to handle spill of render times

        assert(evk.win.Frames == NULL);
        evk.win.Frames = (EasyVkFrame*)malloc(sizeof(EasyVkFrame) * evk.win.ImageCount);
        evk.win.FrameSemaphores = (EasyVkFrameSemaphores*)malloc(sizeof(EasyVkFrameSemaphores) * evk.win.ImageCount);
		evk.win.FrameTimestamps = (EasyVkFrameTimestamps*)malloc(sizeof(EasyVkFrameTimestamps) * evk.win.FrameTimestampsCount);
        memset(evk.win.Frames, 0, sizeof(evk.win.Frames[0]) * evk.win.ImageCount);
        memset(evk.win.FrameSemaphores, 0, sizeof(evk.win.FrameSemaphores[0]) * evk.win.ImageCount);
		memset(evk.win.FrameTimestamps, 0, sizeof(evk.win.FrameSemaphores[0]) * evk.win.FrameTimestampsCount);
        for (uint32_t i = 0; i < evk.win.ImageCount; i++)
            evk.win.Frames[i].Backbuffer = backbuffers[i];
    }
    if (old_swapchain)
        vkDestroySwapchainKHR(evk.dev, old_swapchain, evk.alloc);

    // Create the Render Pass
    {
        VkAttachmentDescription attachment = {};
        attachment.format = evk.win.SurfaceFormat.format;
        attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        attachment.loadOp = evk.win.ClearEnable ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        VkAttachmentReference color_attachment = {};
        color_attachment.attachment = 0;
        color_attachment.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &color_attachment;
        VkSubpassDependency dependency = {};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        VkRenderPassCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        info.attachmentCount = 1;
        info.pAttachments = &attachment;
        info.subpassCount = 1;
        info.pSubpasses = &subpass;
        info.dependencyCount = 1;
        info.pDependencies = &dependency;
        err = vkCreateRenderPass(evk.dev, &info, evk.alloc, &evk.win.RenderPass);
        check_vk_result(err);
    }

    // Create The Image Views
    {
        VkImageViewCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        info.format = evk.win.SurfaceFormat.format;
        info.components.r = VK_COMPONENT_SWIZZLE_R;
        info.components.g = VK_COMPONENT_SWIZZLE_G;
        info.components.b = VK_COMPONENT_SWIZZLE_B;
        info.components.a = VK_COMPONENT_SWIZZLE_A;
        VkImageSubresourceRange image_range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        info.subresourceRange = image_range;
        for (uint32_t i = 0; i < evk.win.ImageCount; i++)
        {
            EasyVkFrame* fd = &evk.win.Frames[i];
            info.image = fd->Backbuffer;
            err = vkCreateImageView(evk.dev, &info, evk.alloc, &fd->BackbufferView);
            check_vk_result(err);
        }
    }

    // Create Framebuffer
    {
        VkImageView attachment[1];
        VkFramebufferCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        info.renderPass = evk.win.RenderPass;
        info.attachmentCount = 1;
        info.pAttachments = attachment;
        info.width = evk.win.Width;
        info.height = evk.win.Height;
        info.layers = 1;
        for (uint32_t i = 0; i < evk.win.ImageCount; i++)
        {
            EasyVkFrame* fd = &evk.win.Frames[i];
            attachment[0] = fd->BackbufferView;
            err = vkCreateFramebuffer(evk.dev, &info, evk.alloc, &fd->Framebuffer);
            check_vk_result(err);
        }
    }

	// Create Command Buffers, Fences, Semaphores and Query Pools
	assert(evk.phys_dev != VK_NULL_HANDLE && evk.dev != VK_NULL_HANDLE);
    for (uint32_t i = 0; i < evk.win.ImageCount; i++)
    {
        EasyVkFrame* fd = &evk.win.Frames[i];
        EasyVkFrameSemaphores* fsd = &evk.win.FrameSemaphores[i];
        {
            VkCommandPoolCreateInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
            info.queueFamilyIndex = evk.que_fam;
            err = vkCreateCommandPool(evk.dev, &info, evk.alloc, &fd->CommandPool);
            check_vk_result(err);
        }
        {
            VkCommandBufferAllocateInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            info.commandPool = fd->CommandPool;
            info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            info.commandBufferCount = 1;
            err = vkAllocateCommandBuffers(evk.dev, &info, &fd->CommandBuffer);
            check_vk_result(err);
        }
        {
            VkFenceCreateInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
            err = vkCreateFence(evk.dev, &info, evk.alloc, &fd->Fence);
            check_vk_result(err);
        }
        {
            VkSemaphoreCreateInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            err = vkCreateSemaphore(evk.dev, &info, evk.alloc, &fsd->ImageAcquiredSemaphore);
            check_vk_result(err);
            err = vkCreateSemaphore(evk.dev, &info, evk.alloc, &fsd->RenderCompleteSemaphore);
            check_vk_result(err);
        }
    }

	// Create Frame Timestamp Query Pools:
	for (uint32_t i = 0; i < evk.win.FrameTimestampsCount; i++) {
		EasyVkFrameTimestamps* ftd = &evk.win.FrameTimestamps[i];		
		VkQueryPoolCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
		info.queryType = VK_QUERY_TYPE_TIMESTAMP;
		info.queryCount = QUERIES_PER_FRAME_MAX;
		err = vkCreateQueryPool(evk.dev, &info, evk.alloc, &ftd->QueryPool);
		check_vk_result(err);	
	}
}
void evkCheckError(VkResult err) {
	check_vk_result(err);
}
EasyVkCheckErrorFunc evkGetCheckErrorFunc() {
	return check_vk_result;
}

void evkResetCommandPool(VkCommandPool command_pool) {
	VkResult err;
    err = vkResetCommandPool(evk.dev, command_pool, 0);
    evkCheckError(err);
}
void evkBeginCommandBuffer(VkCommandBuffer command_buffer) {
	VkResult err;
    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    err = vkBeginCommandBuffer(command_buffer, &begin_info);
    evkCheckError(err);
}
void evkEndCommandBufferAndSubmit(VkCommandBuffer command_buffer) {
    VkResult err;
	VkSubmitInfo end_info = {};
    end_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    end_info.commandBufferCount = 1;
    end_info.pCommandBuffers = &command_buffer;
    err = vkEndCommandBuffer(command_buffer);
    evkCheckError(err);
    err = vkQueueSubmit(evk.que, 1, &end_info, VK_NULL_HANDLE);
    evkCheckError(err);
}
void evkDebugSetObjectName(VkObjectType objectType, uint64_t objectHandle, const char* pObjectName) {
	// doesn't work :(
	/*
	auto vkSetDebugUtilsObjectNameEXT  = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetInstanceProcAddr(evk.inst, "vkSetDebugUtilsObjectNameEXT");
	assert(vkSetDebugUtilsObjectNameEXT  != NULL);
	VkDebugUtilsObjectNameInfoEXT info = {};
	info.objectType = objectType;
	info.objectHandle = objectHandle;
	info.pObjectName = pObjectName;
	vkSetDebugUtilsObjectNameEXT(evk.dev, &info);
	*/
}

static VkSemaphore image_acquired_semaphore = 0;
static VkSemaphore render_complete_semaphore = 0;
static VkCommandBuffer render_command_buffer = 0;

void evkFrameAcquire() {
    VkResult err;

    image_acquired_semaphore  = evk.win.FrameSemaphores[evk.win.SemaphoreIndex].ImageAcquiredSemaphore;
    render_complete_semaphore = evk.win.FrameSemaphores[evk.win.SemaphoreIndex].RenderCompleteSemaphore;
    err = vkAcquireNextImageKHR(evk.dev, evk.win.Swapchain, UINT64_MAX, image_acquired_semaphore, VK_NULL_HANDLE, &evk.win.FrameIndex);
    check_vk_result(err);

	EasyVkFrame* fd = &evk.win.Frames[evk.win.FrameIndex];

	{
        err = vkWaitForFences(evk.dev, 1, &fd->Fence, VK_TRUE, UINT64_MAX);    // wait indefinitely instead of periodically checking
        check_vk_result(err);

        err = vkResetFences(evk.dev, 1, &fd->Fence);
        check_vk_result(err);
    }

	{
        err = vkResetCommandPool(evk.dev, fd->CommandPool, 0);
        check_vk_result(err);
	}

    {
        VkCommandBufferBeginInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        err = vkBeginCommandBuffer(fd->CommandBuffer, &info);
        check_vk_result(err);
    }

	render_command_buffer = fd->CommandBuffer;
}
void evkRenderBegin() {
    VkResult err;

    EasyVkFrame* fd = &evk.win.Frames[evk.win.FrameIndex];
    {
        VkRenderPassBeginInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        info.renderPass = evk.win.RenderPass;
        info.framebuffer = fd->Framebuffer;
        info.renderArea.extent.width = evk.win.Width;
        info.renderArea.extent.height = evk.win.Height;
        info.clearValueCount = 1;
        info.pClearValues = &evk.win.ClearValue;
        vkCmdBeginRenderPass(fd->CommandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);
    }
}
VkCommandBuffer evkGetRenderCommandBuffer() {
	return render_command_buffer;
}
void evkRenderEnd() {
	VkResult err;

	EasyVkFrame* fd = &evk.win.Frames[evk.win.FrameIndex];
	// Submit command buffer
    vkCmdEndRenderPass(fd->CommandBuffer);
    {
        VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        info.waitSemaphoreCount = 1;
        info.pWaitSemaphores = &image_acquired_semaphore;
        info.pWaitDstStageMask = &wait_stage;
        info.commandBufferCount = 1;
        info.pCommandBuffers = &fd->CommandBuffer;
        info.signalSemaphoreCount = 1;
        info.pSignalSemaphores = &render_complete_semaphore;

        err = vkEndCommandBuffer(fd->CommandBuffer);
        check_vk_result(err);
        err = vkQueueSubmit(evk.que, 1, &info, fd->Fence);
        check_vk_result(err);
    }
}

void evkFramePresent() {
    VkSemaphore render_complete_semaphore = evk.win.FrameSemaphores[evk.win.SemaphoreIndex].RenderCompleteSemaphore;
    VkPresentInfoKHR info = {};
    info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    info.waitSemaphoreCount = 1;
    info.pWaitSemaphores = &render_complete_semaphore;
    info.swapchainCount = 1;
    info.pSwapchains = &evk.win.Swapchain;
    info.pImageIndices = &evk.win.FrameIndex;
    VkResult err = vkQueuePresentKHR(evk.que, &info);
    check_vk_result(err);
    evk.win.SemaphoreIndex = (evk.win.SemaphoreIndex + 1) % evk.win.ImageCount; // Now we can use the next set of semaphores
}


static int getMinImageCountFromPresentMode(VkPresentModeKHR present_mode) {
    if (present_mode == VK_PRESENT_MODE_MAILBOX_KHR)
        return 3;
    if (present_mode == VK_PRESENT_MODE_FIFO_KHR || present_mode == VK_PRESENT_MODE_FIFO_RELAXED_KHR)
        return 2;
    if (present_mode == VK_PRESENT_MODE_IMMEDIATE_KHR)
        return 1;
    assert(0);
    return 1;
}

static void destroyFrame(EasyVkFrame* fd) {
	vkDestroyFence(evk.dev, fd->Fence, evk.alloc);
    vkFreeCommandBuffers(evk.dev, fd->CommandPool, 1, &fd->CommandBuffer);
    vkDestroyCommandPool(evk.dev, fd->CommandPool, evk.alloc);
	
    fd->Fence = VK_NULL_HANDLE;
    fd->CommandBuffer = VK_NULL_HANDLE;
    fd->CommandPool = VK_NULL_HANDLE;

    vkDestroyImageView(evk.dev, fd->BackbufferView, evk.alloc);
    vkDestroyFramebuffer(evk.dev, fd->Framebuffer, evk.alloc);
}
static void destroyFrameSemaphores(EasyVkFrameSemaphores* fsd) {
	vkDestroySemaphore(evk.dev, fsd->ImageAcquiredSemaphore, evk.alloc);
    vkDestroySemaphore(evk.dev, fsd->RenderCompleteSemaphore, evk.alloc);
    fsd->ImageAcquiredSemaphore = fsd->RenderCompleteSemaphore = VK_NULL_HANDLE;
}
static void destroyFrameTimestamps(EasyVkFrameTimestamps* ftd) {
	vkDestroyQueryPool(evk.dev, ftd->QueryPool, evk.alloc);
}
static void destroyAllFramesAndSemaphoresAndTimestamps() {
	for (uint32_t i = 0; i < evk.win.ImageCount; i++) {
        destroyFrame(&evk.win.Frames[i]);
        destroyFrameSemaphores(&evk.win.FrameSemaphores[i]);
    }
	for (uint32_t i = 0; i < evk.win.ImageCount; i++)
		destroyFrameTimestamps(&evk.win.FrameTimestamps[i]);
	free(evk.win.Frames);
    free(evk.win.FrameSemaphores);
	free(evk.win.FrameTimestamps);
    evk.win.Frames = NULL;
    evk.win.FrameSemaphores = NULL;
    evk.win.ImageCount = 0;
	evk.win.FrameTimestampsCount = 0;
}

#include "imgui/imgui.h"

static void evkTimerGUI(int64_t time_of_prev_frame_start, int64_t time_of_frame_start, int64_t* times, int times_count) {
	const float ms_per_frame = 1000.0f / 144.0f;
	int64_t del = time_of_frame_start - time_of_prev_frame_start;
	float frame_ms = double(del) / 1000000.0 * evk.phys_props.limits.timestampPeriod;

	static bool show_timeline = true;

	// apply hysteresis to frame_ms:
	// ignore frames that are insanely long or that aren't that much smaller than before
	static int frame_range_hyst = 0;
	int frame_range = int(frame_ms / ms_per_frame) + 1;
	if (frame_range < 16 && frame_range > frame_range_hyst)
		frame_range_hyst = frame_range;
	else if (frame_range < (frame_range_hyst - 1))
		frame_range_hyst = frame_range;
		
	const double nanosec_per_count = evk.phys_props.limits.timestampPeriod;
	const double ms_per_count = nanosec_per_count / 1000000.0;
	const char* texts[] = { "update", "clear stats", "render", "draw", "swap", "other", "last" };
	//ImU32 label_cols[ARRSIZE(texts)] = { 0x80404080, 0x80408040, 0x80804040, 0x80408080, 0x80804080, 0x80808040};
	ImU32 label_cols[ARRSIZE(texts)] = { 0xff404080, 0xff408040, 0xff804040, 0xff408080, 0xff804080, 0xff808040};
	int label_chars_max = 0;
	for (int i = 0; i < ARRSIZE(texts); ++i)
		label_chars_max = max(label_chars_max, (int)strlen(texts[i]));
	const int label_count = times_count-1;
	const float pixels_from_ms = 15.0f; 
	const float bars_x_start = float(label_chars_max + 9) * gui::GetFont()->FallbackAdvanceX;

	gui::SetNextWindowSize(vec2(show_timeline ? 800.0f : bars_x_start + gui::GetStyle().WindowPadding.x, 0.0f));
	if (gui::Begin("GPU Timers", 0, ImGuiWindowFlags_AlwaysAutoResize)) {
		if (show_timeline && gui::Button("collapse", vec2(bars_x_start - gui::GetFont()->FallbackAdvanceX, 0.f))) show_timeline = false;
		else if (!show_timeline && gui::Button("expand", vec2(bars_x_start - gui::GetFont()->FallbackAdvanceX, 0.f))) show_timeline = true;
		if (frame_ms < 0.0) {
			log("Strange frametime: %6.3f (curr=%lld [0x%x] prev=%lld [0x%x])\n", frame_ms, time_of_frame_start, time_of_frame_start, time_of_prev_frame_start, time_of_prev_frame_start);
		}

		ImDrawList* dl = gui::GetWindowDrawList();
		const vec2 cs = gui::GetContentRegionAvail();

		gui::Text("%*s  %6.3f", -label_chars_max, "frame", frame_ms);

		const ImU32 major_grid_col = 0xff888888;
		const ImU32 minor_grid_col = 0xff444444;
		const ImU32 frame_grid_col = 0xff444488;
		const ImU32 bar_col = 0x80eeeeee;
		// Draw the grid:
		if (show_timeline) {
			const vec2 cp = gui::GetCursorScreenPos();
			// starting line:
			dl->AddLine(cp + vec2(bars_x_start, 0.0f), cp + vec2(bars_x_start, (label_count-1) * gui::GetTextLineHeightWithSpacing() + gui::GetTextLineHeight()), major_grid_col);
			float ms = 1.0f;

			while (ms < (frame_range_hyst * ms_per_frame)) {
				float x = bars_x_start + ms * pixels_from_ms;
				dl->AddLine(cp + vec2(x, 0.0f), cp + vec2(x, (label_count-1) * gui::GetTextLineHeightWithSpacing() + gui::GetTextLineHeight()), minor_grid_col);
				ms += 1.0f;
			}
			float x = bars_x_start + ms_per_frame * pixels_from_ms;
			dl->AddLine(cp + vec2(x, 0.0f), cp + vec2(x, (label_count-1) * gui::GetTextLineHeightWithSpacing() + gui::GetTextLineHeight()), frame_grid_col);
		}
		
		const vec2 cp_line = gui::GetCursorScreenPos();
		for (int i = 0; i < label_count; ++i) {
			int64_t del = times[i+1] - times[i];
			float ms = double(del) * nanosec_per_count / 1000000.0;

			const vec2 cp = gui::GetCursorScreenPos();
			//if (show_timeline)
				dl->AddRectFilled(cp, cp + vec2(bars_x_start - gui::GetFont()->FallbackAdvanceX, gui::GetTextLineHeightWithSpacing()), label_cols[i]);
			gui::Text("%*s  %6.3f", -label_chars_max, texts[i%ARRSIZE(texts)], ms);
			if (show_timeline) {
				/* feels redundant if there is room to draw over label
				if (gui::GetMousePos().x >= (cp.x + bars_x_start) &&
					gui::GetMousePos().y >= cp.y && gui::GetMousePos().y < (cp.y + gui::GetTextLineHeightWithSpacing())) {
					gui::BeginTooltip();
					gui::Text("%s: %6.3f", texts[i%ARRSIZE(texts)], ms);
					gui::EndTooltip();
				}
				*/
				const vec2 cp = cp_line;
				vec2 s = vec2(bars_x_start + double(times[i] - time_of_frame_start) * ms_per_count * pixels_from_ms, 0.0f);
				vec2 e = vec2(bars_x_start + double(times[i+1] - time_of_frame_start) * ms_per_count * pixels_from_ms, gui::GetTextLineHeightWithSpacing());
				dl->AddRectFilled(cp + s, cp + e, label_cols[i]);
				//dl->AddRect(cp + s, cp + e, major_grid_col); // nice to see "something" for all the pieces, but also a huge false impression of "negilible" vs "very small but existent"

				// nice to know the exact number, but without also seeing the label you can't tell what the number belongs to, so still have to look over. tooltip of "label: time" is better (though requires mouse)
				// actually really nice if we use color, because easy to remember "blue is update" etc.
				if (ms > 1.0) {
					TempStr digits = TempStr("%.1f", ms);
					dl->AddText(cp + vec2((s.x + e.x)*0.5f - gui::GetFont()->FallbackAdvanceX * digits.len * 0.5f, s.y), 0xffffffff, digits.str);
					//dl->AddText(cp + vec2(e.x, s.y), 0xffffffff, digits.str);
					//dl->AddText(cp + s - vec2((strlen(texts[i]) * gui::GetFont()->FallbackAdvanceX), 0.0f), 0xffffffff, texts[i%ARRSIZE(texts)]);
				}
				
			}
		}
	} gui::End();
}
void evkTimeFrameGet() {
	// Read results from the last frame in the chain (FrameIdx + 1) % count = tail)
	EasyVkFrameTimestamps* ft_prev = &evk.win.FrameTimestamps[(evk.win.TimestampIndex + 1) % evk.win.FrameTimestampsCount];
	EasyVkFrameTimestamps* ft = &evk.win.FrameTimestamps[(evk.win.TimestampIndex + 2) % evk.win.FrameTimestampsCount];
	int64_t times[QUERIES_PER_FRAME_MAX];
	VkResult res = vkGetQueryPoolResults(evk.dev, ft->QueryPool, 0, ft->QueryCount, sizeof(times), times, sizeof(int64_t), VK_QUERY_RESULT_64_BIT);
	if (res != VK_SUCCESS) {
		log("Timestamp: %s\n", errorString(res));
	}
	int64_t times_prev[QUERIES_PER_FRAME_MAX];
	VkResult res_prev = vkGetQueryPoolResults(evk.dev, ft_prev->QueryPool, 0, ft_prev->QueryCount, sizeof(times_prev), times_prev, sizeof(int64_t), VK_QUERY_RESULT_64_BIT);
	if (res_prev != VK_SUCCESS) {
		log("Timestamp Prev: %s\n", errorString(res_prev));
	}
	evkTimerGUI(times_prev[0], times[0], times+1, ft->QueryCount-1);
}
void evkTimeFrameReset() {
	evk.win.TimestampIndex = (evk.win.TimestampIndex + 1) % evk.win.FrameTimestampsCount;
	EasyVkFrameTimestamps* ft = &evk.win.FrameTimestamps[evk.win.TimestampIndex];
	ft->QueryCount = 0;
	vkCmdResetQueryPool(render_command_buffer, ft->QueryPool, 0, QUERIES_PER_FRAME_MAX); //#OPT does it help to move this to right after you consume the data?
	evkTimeQuery(); // start of frame
}
int evkTimeQuery(VkPipelineStageFlagBits stage) {
	EasyVkFrameTimestamps* ft = &evk.win.FrameTimestamps[evk.win.TimestampIndex];
	assert(ft->QueryCount < QUERIES_PER_FRAME_MAX);
	if (ft->QueryCount >= QUERIES_PER_FRAME_MAX) {
		return -1;
	}
	vkCmdWriteTimestamp(render_command_buffer, stage, ft->QueryPool, ft->QueryCount++);
	return ft->QueryCount-1;
}

void evkWaitUntilDeviceIdle() {
	VkResult err;
	err = vkDeviceWaitIdle(evk.dev);
	evkCheckError(err);
}
void evkWaitUntilReadyToTerm() {
	evkWaitUntilDeviceIdle();
}
void evkMemoryBarrier(VkCommandBuffer cb, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMsk, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask) {
	VkMemoryBarrier bar[1] = {};
	bar[0].sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	bar[0].srcAccessMask = srcAccessMask;
	bar[0].dstAccessMask = dstAccessMsk;
	vkCmdPipelineBarrier(cb, srcStageMask, dstStageMask, 0, 1, bar, 0, NULL, 0, NULL);
}