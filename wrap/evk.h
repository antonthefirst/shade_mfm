/* An easy and hackable implementation of a basic vulkan setup         */
/* Heavily borrowed from Dear ImGui's vulkan example, thanks Omar! <3  */

#pragma once
#include <vulkan/vulkan.h>
#include "core/basic_types.h"

// #TODO replace with own structs
#include "imgui/imgui.h"
#include "wrap/imgui_impl_vulkan.h"

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
	ImGui_ImplVulkanH_Window win;
};

typedef void (*EasyVkCheckErrorFunc)(VkResult err);

extern EasyVk evk;

void evkInit(const char** extensions, uint32_t extensions_count);
void evkTerm();

uint32_t evkMemoryType(VkMemoryPropertyFlags properties, uint32_t type_bits);
int  evkMinImageCount();
void evkSelectSurfaceFormatAndPresentMode(VkSurfaceKHR surface);
void evkResizeWindow(ivec2 res);
void evkCheckError(VkResult err);
EasyVkCheckErrorFunc evkGetCheckErrorFunc();
//void evkDebugSetObjectName(VkObjectType objectType, uint64_t objectHandle, const char* pObjectName);

void evkResetCommandPool(VkCommandPool command_pool);
void evkBeginCommandBuffer(VkCommandBuffer command_buffer);
void evkEndCommandBufferAndSubmit(VkCommandBuffer command_buffer);

void evkFrameAcquire();
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