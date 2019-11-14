/* An easy and hackable implementation of a basic vulkan setup         */
/* Heavily borrowed from Dear ImGui's vulkan example, thanks Omar! <3  */

#pragma once
#include <vulkan/vulkan.h>
#include "core/vec2.h"
#include "core/basic_types.h"
#include "core/string_range.h" // for shader compiler output


// Helper structure to hold the data needed by one rendering frame
// (Used by example's main.cpp. Used by multi-viewport features. Probably NOT used by your own engine/app.)
// [Please zero-clear before use!]
struct EasyVkFrame
{
	VkCommandPool       CommandPool;
	VkCommandBuffer     CommandBuffer;
	VkFence             Fence;
	VkImage             Backbuffer;
	VkImageView         BackbufferView;
	VkFramebuffer       Framebuffer;
};

struct EasyVkFrameTimestamps {
	VkQueryPool			QueryPool;
	int                 QueryCount;
};

struct EasyVkFrameSemaphores
{
	VkSemaphore         ImageAcquiredSemaphore;
	VkSemaphore         RenderCompleteSemaphore;
};

// Helper structure to hold the data needed by one rendering context into one OS window
// (Used by example's main.cpp. Used by multi-viewport features. Probably NOT used by your own engine/app.)
struct EasyVkWindow
{
	int                 Width;
	int                 Height;
	int					RefreshRate;
	VkSwapchainKHR      Swapchain;
	VkSurfaceKHR        Surface;
	VkSurfaceFormatKHR  SurfaceFormat;
	VkPresentModeKHR    PresentMode;
	VkRenderPass        RenderPass;
	bool                ClearEnable;
	VkClearValue        ClearValue;
	uint32_t            FrameIndex;             // Current frame being rendered to (0 <= FrameIndex < FrameInFlightCount)
	uint32_t            ImageCount;             // Number of simultaneous in-flight frames (returned by vkGetSwapchainImagesKHR, usually derived from min_image_count)
	uint32_t            SemaphoreIndex;         // Current set of swapchain wait semaphores we're using (needs to be distinct from per frame data)
	EasyVkFrame*            Frames;
	EasyVkFrameSemaphores*  FrameSemaphores;    

	EasyVkWindow();
};

struct EasyVk {
	VkInstance inst;
	VkAllocationCallbacks* alloc;
	VkPhysicalDevice phys_dev;
	VkDevice dev;
	uint32_t que_fam;
	VkQueue que;
	VkDescriptorPool desc_pool;
	VkPipelineCache pipe_cache;
	VkDebugReportCallbackEXT debug;
	EasyVkWindow win;
	VkPhysicalDeviceProperties phys_props;
	VkQueueFamilyProperties que_fam_props;
};

typedef void (*EasyVkCheckErrorFunc)(VkResult err);

extern EasyVk evk;

void evkInit(const char** extensions, uint32_t extensions_count);
void evkTerm();
void evkNotifyOfWindow(ivec2 new_resolution);
bool evkCheckForSwapchainChanges();
bool evkWindowIsMinimized();

VkShaderModule evkCreateShaderFromFile(const char* pathfile, String* output = NULL);

uint32_t evkMemoryType(VkMemoryPropertyFlags properties, uint32_t type_bits);
int  evkMinImageCount();
void evkSelectSurfaceFormatAndPresentMode(VkSurfaceKHR surface);
void evkResizeWindow(ivec2 res, int refresh_rate);
void evkCheckError(VkResult err);
EasyVkCheckErrorFunc evkGetCheckErrorFunc();

void evkResetCommandPool(VkCommandPool command_pool);
void evkBeginCommandBuffer(VkCommandBuffer command_buffer);
void evkEndCommandBufferAndSubmit(VkCommandBuffer command_buffer);

bool evkFrameAcquire();
void evkRenderBegin();
VkCommandBuffer evkGetRenderCommandBuffer();
void evkRenderEnd();
void evkFramePresent();

void evkWaitUntilDeviceIdle();
void evkWaitUntilReadyToTerm();
void evkMemoryBarrier(VkCommandBuffer cb,
                      VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMsk,
                      VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask);

inline
VkDescriptorSetLayoutBinding evkMakeDescriptorSetLayoutBinding(VkDescriptorType type,
                                                               VkShaderStageFlags stageFlags,
                                                               uint32_t binding,
                                                               uint32_t descriptorCount = 1) {
	VkDescriptorSetLayoutBinding setLayoutBinding {};
	setLayoutBinding.descriptorType = type;
	setLayoutBinding.stageFlags = stageFlags;
	setLayoutBinding.binding = binding;
	setLayoutBinding.descriptorCount = descriptorCount;
	return setLayoutBinding;
}

inline
VkDescriptorSetLayoutCreateInfo evkMakeDescriptorSetLayoutCreateInfo(const VkDescriptorSetLayoutBinding* pBindings,
                                                                     uint32_t bindingCount) {
	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo {};
	descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptorSetLayoutCreateInfo.pBindings = pBindings;
	descriptorSetLayoutCreateInfo.bindingCount = bindingCount;
	return descriptorSetLayoutCreateInfo;
}

inline
VkPipelineLayoutCreateInfo evkMakePipelineLayoutCreateInfo(const VkDescriptorSetLayout* pSetLayouts,
                                                           uint32_t setLayoutCount = 1) {
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo {};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.setLayoutCount = setLayoutCount;
	pipelineLayoutCreateInfo.pSetLayouts = pSetLayouts;
	return pipelineLayoutCreateInfo;
}

inline
VkDescriptorSetAllocateInfo evkMakeDescriptorSetAllocateInfo(VkDescriptorPool descriptorPool,
                                                             const VkDescriptorSetLayout* pSetLayouts,
                                                             uint32_t descriptorSetCount) {
	VkDescriptorSetAllocateInfo descriptorSetAllocateInfo {};
	descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	descriptorSetAllocateInfo.descriptorPool = descriptorPool;
	descriptorSetAllocateInfo.pSetLayouts = pSetLayouts;
	descriptorSetAllocateInfo.descriptorSetCount = descriptorSetCount;
	return descriptorSetAllocateInfo;
}

inline
VkWriteDescriptorSet evkMakeWriteDescriptorSet(VkDescriptorSet dstSet,
                                               VkDescriptorType type,
                                               uint32_t binding,
                                               VkDescriptorBufferInfo* bufferInfo,
                                               uint32_t descriptorCount = 1) {
	VkWriteDescriptorSet writeDescriptorSet {};
	writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeDescriptorSet.dstSet = dstSet;
	writeDescriptorSet.descriptorType = type;
	writeDescriptorSet.dstBinding = binding;
	writeDescriptorSet.pBufferInfo = bufferInfo;
	writeDescriptorSet.descriptorCount = descriptorCount;
	return writeDescriptorSet;
}

inline
VkWriteDescriptorSet evkMakeWriteDescriptorSet(VkDescriptorSet dstSet,
                                               VkDescriptorType type,
                                               uint32_t binding,
                                               VkDescriptorImageInfo *imageInfo,
                                               uint32_t descriptorCount = 1) {
	VkWriteDescriptorSet writeDescriptorSet {};
	writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeDescriptorSet.dstSet = dstSet;
	writeDescriptorSet.descriptorType = type;
	writeDescriptorSet.dstBinding = binding;
	writeDescriptorSet.pImageInfo = imageInfo;
	writeDescriptorSet.descriptorCount = descriptorCount;
	return writeDescriptorSet;
}

inline
VkComputePipelineCreateInfo evkMakeComputePipelineCreateInfo(VkPipelineLayout layout, 
                                                      VkPipelineCreateFlags flags = 0) {
	VkComputePipelineCreateInfo computePipelineCreateInfo {};
	computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	computePipelineCreateInfo.layout = layout;
	computePipelineCreateInfo.flags = flags;
	return computePipelineCreateInfo;
}