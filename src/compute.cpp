#include "wrap/evk.h"
#include "core/log.h"
#include "core/file_stat.h"
#include "shaders/cpu_gpu_shared.inl"

static VkPipelineCreateFlags    g_ComputePipelineCreateFlags = 0x00;
static VkDescriptorSetLayout    g_ComputeDescriptorSetLayout = VK_NULL_HANDLE;
static VkPipelineLayout         g_ComputePipelineLayout = VK_NULL_HANDLE;
static VkDescriptorSet          g_ComputeDescriptorSet = VK_NULL_HANDLE;
static VkPipeline               g_ComputePipeline = VK_NULL_HANDLE;

static ComputeUPC upc;

void computeRecreatePipelineIfNeeded() {
	if (g_ComputePipeline) return;

    VkResult err;
    VkShaderModule comp_module;
    // Create The Shader Module:
    {
        VkShaderModuleCreateInfo comp_info = {};
        comp_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		comp_info.pCode = (uint32_t*)fileReadBinaryIntoMem("shaders/update.comp.spv", &comp_info.codeSize);
		//comp_info.pCode = (uint32_t*)fileReadBinaryIntoMem("shaders/basic.comp.spv", &comp_info.codeSize);
        err = vkCreateShaderModule(evk.dev, &comp_info, evk.alloc, &comp_module);
		free((void*)comp_info.pCode);
    }

	// Create Descriptor Set Layout
	{
		VkDescriptorSetLayoutBinding setLayoutBindings[] = {
			evkMakeDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 0),
			evkMakeDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 1),
			evkMakeDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 2),
			evkMakeDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 3),
			evkMakeDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 4),
			evkMakeDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 5),
		};
		VkDescriptorSetLayoutCreateInfo descriptorLayout = evkMakeDescriptorSetLayoutCreateInfo(setLayoutBindings, ARRSIZE(setLayoutBindings));
		err = vkCreateDescriptorSetLayout(evk.dev, &descriptorLayout, nullptr, &g_ComputeDescriptorSetLayout);
		evkCheckError(err);
	}

	// Create Pipeline Layout
	{
		VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo = evkMakePipelineLayoutCreateInfo(&g_ComputeDescriptorSetLayout);
		VkPushConstantRange push_constants[1] = {};
        push_constants[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        push_constants[0].size = sizeof(ComputeUPC);
		pPipelineLayoutCreateInfo.pushConstantRangeCount = 1;
		pPipelineLayoutCreateInfo.pPushConstantRanges = push_constants;
		err = vkCreatePipelineLayout(evk.dev, &pPipelineLayoutCreateInfo, nullptr, &g_ComputePipelineLayout);
		evkCheckError(err);
	}

	// Create Descriptor Set
	{
		VkDescriptorSetAllocateInfo allocInfo = evkMakeDescriptorSetAllocateInfo(evk.desc_pool, &g_ComputeDescriptorSetLayout, 1);
		err = vkAllocateDescriptorSets(evk.dev, &allocInfo, &g_ComputeDescriptorSet);
		evkCheckError(err);

		//#TODO update descriptor set here? right now updated as part of tex load...
	}

	// Create compute shader pipeline
	{
		VkComputePipelineCreateInfo computePipelineCreateInfo = evkMakeComputePipelineCreateInfo(g_ComputePipelineLayout);
		VkPipelineShaderStageCreateInfo stage = {};
		stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
		stage.module = comp_module;
		stage.pName = "main";
		computePipelineCreateInfo.stage = stage;
		vkCreateComputePipelines(evk.dev, evk.pipe_cache, 1, &computePipelineCreateInfo, evk.alloc, &g_ComputePipeline);
		evkCheckError(err);
	}
	
    vkDestroyShaderModule(evk.dev, comp_module, evk.alloc);
}
VkDescriptorSet computeGetDescriptorSet() {
	return g_ComputeDescriptorSet;
}
void computeDestroy() {
    if (g_ComputeDescriptorSetLayout)  { vkDestroyDescriptorSetLayout(evk.dev, g_ComputeDescriptorSetLayout, evk.alloc); g_ComputeDescriptorSetLayout = VK_NULL_HANDLE; }
    if (g_ComputePipelineLayout)       { vkDestroyPipelineLayout(evk.dev, g_ComputePipelineLayout, evk.alloc); g_ComputePipelineLayout = VK_NULL_HANDLE; }
    if (g_ComputePipeline)             { vkDestroyPipeline(evk.dev, g_ComputePipeline, evk.alloc); g_ComputePipeline = VK_NULL_HANDLE; }
}

void computeBegin(VkCommandBuffer command_buffer) {
	// Bind pipeline
	vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, g_ComputePipeline);
	
	// Bind descriptor sets
	vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, g_ComputePipelineLayout, 0, 1, &g_ComputeDescriptorSet, 0, 0);
	
}
void computeStage(VkCommandBuffer command_buffer, int stage, ivec2 size) {
	ivec2 dispatch = ivec2((size.x / GROUP_SIZE_X) + 1, (size.y / GROUP_SIZE_Y) + 1);
	upc.stage = stage;
	vkCmdPushConstants(command_buffer, g_ComputePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputeUPC) , &upc);
	vkCmdDispatch(command_buffer, dispatch.x, dispatch.y, 1);
	evkMemoryBarrier(command_buffer, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
}
/*
void computeUpdate(VkCommandBuffer cb) {

	if (do_reset) {
		do_reset = false;
		upc.stage = 0;
		vkCmdPushConstants(command_buffer, g_ComputePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputeUPC) , &upc);
		vkCmdDispatch(command_buffer, total_map.size.x / 16, total_map.size.y / 16, 1);
		evkMemoryBarrier(command_buffer, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
	}
	
	if (do_update) {
		for (int i = 0; i < 1; ++i) {
			upc.stage = 1;
			vkCmdPushConstants(command_buffer, g_ComputePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputeUPC) , &upc);
			vkCmdDispatch(command_buffer, total_map.size.x / 16, total_map.size.y / 16, 1);
			evkMemoryBarrier(command_buffer, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
		}
	}
	
	if (do_render) {
		upc.stage = 2;
		vkCmdPushConstants(command_buffer, g_ComputePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputeUPC) , &upc);
		vkCmdDispatch(command_buffer, total_map.size.x / 16, total_map.size.y / 16, 1);
		evkMemoryBarrier(command_buffer, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
	}
	
}
*/