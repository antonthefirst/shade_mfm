#include "evk.h"
#include <string.h>   // for memcpy
#include "core/log.h" // for assert

static int getMinImageCountFromPresentMode(VkPresentModeKHR present_mode);
static void destroyAllFramesAndSemaphores();

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
	destroyAllFramesAndSemaphores();
    vkDestroyRenderPass(evk.dev, evk.win.RenderPass, evk.alloc);
    vkDestroySwapchainKHR(evk.dev, evk.win.Swapchain, evk.alloc);
    vkDestroySurfaceKHR(evk.inst, evk.win.Surface, evk.alloc);

    evk.win = ImGui_ImplVulkanH_Window();

	// Destroy core vulkan objects
	vkDestroyDescriptorPool(evk.dev, evk.desc_pool, evk.alloc);
    auto vkDestroyDebugReportCallbackEXT = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(evk.inst, "vkDestroyDebugReportCallbackEXT");
    vkDestroyDebugReportCallbackEXT(evk.inst, evk.debug, evk.alloc);
    vkDestroyDevice(evk.dev, evk.alloc);
    vkDestroyInstance(evk.inst, evk.alloc);
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
	evk.win.SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(evk.phys_dev, evk.win.Surface, requestSurfaceImageFormat, (size_t)ARRSIZE(requestSurfaceImageFormat), requestSurfaceColorSpace);

	// Select Present Mode
	#ifdef UNLIMITED_FRAME_RATE
	VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR, VK_PRESENT_MODE_FIFO_KHR };
	#else
	VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_FIFO_KHR };
	#endif
	evk.win.PresentMode = ImGui_ImplVulkanH_SelectPresentMode(evk.phys_dev, evk.win.Surface, &present_modes[0], ARRSIZE(present_modes));
	logInfo("VK", "Selected PresentMode = %d", evk.win.PresentMode);
}
void evkResizeWindow(ivec2 res) {
    VkResult err;
    VkSwapchainKHR old_swapchain = evk.win.Swapchain;
    err = vkDeviceWaitIdle(evk.dev);
    check_vk_result(err);

    // Destroy old Framebuffer
    // We don't use DestroyWindow() because we want to preserve the old swapchain to create the new one.
	destroyAllFramesAndSemaphores();
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

        assert(evk.win.Frames == NULL);
        evk.win.Frames = (ImGui_ImplVulkanH_Frame*)malloc(sizeof(ImGui_ImplVulkanH_Frame) * evk.win.ImageCount);
        evk.win.FrameSemaphores = (ImGui_ImplVulkanH_FrameSemaphores*)malloc(sizeof(ImGui_ImplVulkanH_FrameSemaphores) * evk.win.ImageCount);
        memset(evk.win.Frames, 0, sizeof(evk.win.Frames[0]) * evk.win.ImageCount);
        memset(evk.win.FrameSemaphores, 0, sizeof(evk.win.FrameSemaphores[0]) * evk.win.ImageCount);
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
            ImGui_ImplVulkanH_Frame* fd = &evk.win.Frames[i];
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
            ImGui_ImplVulkanH_Frame* fd = &evk.win.Frames[i];
            attachment[0] = fd->BackbufferView;
            err = vkCreateFramebuffer(evk.dev, &info, evk.alloc, &fd->Framebuffer);
            check_vk_result(err);
        }
    }

	// Create Command Buffers, Fences and Semaphores
	assert(evk.phys_dev != VK_NULL_HANDLE && evk.dev != VK_NULL_HANDLE);
    for (uint32_t i = 0; i < evk.win.ImageCount; i++)
    {
        ImGui_ImplVulkanH_Frame* fd = &evk.win.Frames[i];
        ImGui_ImplVulkanH_FrameSemaphores* fsd = &evk.win.FrameSemaphores[i];
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

	ImGui_ImplVulkanH_Frame* fd = &evk.win.Frames[evk.win.FrameIndex];

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

    ImGui_ImplVulkanH_Frame* fd = &evk.win.Frames[evk.win.FrameIndex];
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

	ImGui_ImplVulkanH_Frame* fd = &evk.win.Frames[evk.win.FrameIndex];
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

static void destroyFrame(ImGui_ImplVulkanH_Frame* fd) {
	vkDestroyFence(evk.dev, fd->Fence, evk.alloc);
    vkFreeCommandBuffers(evk.dev, fd->CommandPool, 1, &fd->CommandBuffer);
    vkDestroyCommandPool(evk.dev, fd->CommandPool, evk.alloc);
    fd->Fence = VK_NULL_HANDLE;
    fd->CommandBuffer = VK_NULL_HANDLE;
    fd->CommandPool = VK_NULL_HANDLE;

    vkDestroyImageView(evk.dev, fd->BackbufferView, evk.alloc);
    vkDestroyFramebuffer(evk.dev, fd->Framebuffer, evk.alloc);
}
static void destroyFrameSemaphores(ImGui_ImplVulkanH_FrameSemaphores* fsd) {
	vkDestroySemaphore(evk.dev, fsd->ImageAcquiredSemaphore, evk.alloc);
    vkDestroySemaphore(evk.dev, fsd->RenderCompleteSemaphore, evk.alloc);
    fsd->ImageAcquiredSemaphore = fsd->RenderCompleteSemaphore = VK_NULL_HANDLE;
}
static void destroyAllFramesAndSemaphores() {
	for (uint32_t i = 0; i < evk.win.ImageCount; i++) {
        destroyFrame(&evk.win.Frames[i]);
        destroyFrameSemaphores(&evk.win.FrameSemaphores[i]);
    }
	free(evk.win.Frames);
    free(evk.win.FrameSemaphores);
    evk.win.Frames = NULL;
    evk.win.FrameSemaphores = NULL;
    evk.win.ImageCount = 0;
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