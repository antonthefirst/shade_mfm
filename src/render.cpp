#include "render.h"
#include "wrap/evk.h"
#include "core/log.h"
#include "core/file_stat.h"
#include <stdio.h>

#include "shaders/draw_shared.inl"

namespace {

typedef unsigned short DrawIdx;
struct DrawVert {
    vec2  pos;
    vec2  uv;
    //u32   col;
};

struct FrameRenderBuffers {
    VkDeviceMemory      VertexBufferMemory;
    VkDeviceMemory      IndexBufferMemory;
    VkDeviceSize        VertexBufferSize;
    VkDeviceSize        IndexBufferSize;
    VkBuffer            VertexBuffer;
    VkBuffer            IndexBuffer;
};
struct WindowRenderBuffers {
    uint32_t            Index;
    uint32_t            Count;
    FrameRenderBuffers*   Frames;
};

struct BasicQuad {
	DrawVert verts[4] = {
		{vec2(0.0f,0.0f), vec2(0.0f, 0.0f)},
		{vec2(1.0f,0.0f), vec2(1.0f, 0.0f)},
		{vec2(0.0f,1.0f), vec2(0.0f, 1.0f)},
		{vec2(1.0f,1.0f), vec2(1.0f, 1.0f)},
	};
	DrawIdx  idxs[6] = { 0, 1, 2,  1, 3 , 2 };
};

}

static VkDeviceSize             g_BufferMemoryAlignment = 256;
static VkPipelineCreateFlags    g_PipelineCreateFlags = 0x00;
static VkDescriptorSetLayout    g_DescriptorSetLayout = VK_NULL_HANDLE;
static VkPipelineLayout         g_PipelineLayout = VK_NULL_HANDLE;
static VkDescriptorSet          g_DescriptorSet = VK_NULL_HANDLE;
static VkPipeline               g_Pipeline = VK_NULL_HANDLE;
static VkSampler                g_Sampler = VK_NULL_HANDLE;

static WindowRenderBuffers      g_MainWindowRenderBuffers;

static BasicQuad quad;

static void createOrResizeVertexBuffer(VkBuffer& buffer, VkDeviceMemory& buffer_memory, VkDeviceSize& p_buffer_size, size_t new_size, VkBufferUsageFlagBits usage) {
    VkResult err;
    if (buffer != VK_NULL_HANDLE)
        vkDestroyBuffer(evk.dev, buffer, evk.alloc);
    if (buffer_memory != VK_NULL_HANDLE)
        vkFreeMemory(evk.dev, buffer_memory, evk.alloc);

    VkDeviceSize vertex_buffer_size_aligned = ((new_size - 1) / g_BufferMemoryAlignment + 1) * g_BufferMemoryAlignment;
    VkBufferCreateInfo buffer_info = {};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = vertex_buffer_size_aligned;
    buffer_info.usage = usage;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    err = vkCreateBuffer(evk.dev, &buffer_info, evk.alloc, &buffer);
    evkCheckError(err);

    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(evk.dev, buffer, &req);
    g_BufferMemoryAlignment = (g_BufferMemoryAlignment > req.alignment) ? g_BufferMemoryAlignment : req.alignment;
    VkMemoryAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = req.size;
    alloc_info.memoryTypeIndex = evkMemoryType(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, req.memoryTypeBits);
    err = vkAllocateMemory(evk.dev, &alloc_info, evk.alloc, &buffer_memory);
    evkCheckError(err);

    err = vkBindBufferMemory(evk.dev, buffer, buffer_memory, 0);
    evkCheckError(err);
    p_buffer_size = new_size;
}
static void destroyFrameRenderBuffers(FrameRenderBuffers* buffers) {
    if (buffers->VertexBuffer) { vkDestroyBuffer(evk.dev, buffers->VertexBuffer, evk.alloc); buffers->VertexBuffer = VK_NULL_HANDLE; }
    if (buffers->VertexBufferMemory) { vkFreeMemory(evk.dev, buffers->VertexBufferMemory, evk.alloc); buffers->VertexBufferMemory = VK_NULL_HANDLE; }
    if (buffers->IndexBuffer) { vkDestroyBuffer(evk.dev, buffers->IndexBuffer, evk.alloc); buffers->IndexBuffer = VK_NULL_HANDLE; }
    if (buffers->IndexBufferMemory) { vkFreeMemory(evk.dev, buffers->IndexBufferMemory, evk.alloc); buffers->IndexBufferMemory = VK_NULL_HANDLE; }
    buffers->VertexBufferSize = 0;
    buffers->IndexBufferSize = 0;
}

static void destroyWindowRenderBuffers(WindowRenderBuffers* buffers) {
    for (uint32_t n = 0; n < buffers->Count; n++)
        destroyFrameRenderBuffers(&buffers->Frames[n]);
    free(buffers->Frames);
    buffers->Frames = NULL;
    buffers->Index = 0;
    buffers->Count = 0;
}

static void setupRenderState(VkCommandBuffer command_buffer, FrameRenderBuffers* rb, ivec2 fb_size, ivec2 world_size, pose camera_from_world, RenderVis vis)
{
    // Bind pipeline and descriptor sets:
    {
        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, g_Pipeline);
        VkDescriptorSet desc_set[1] = { g_DescriptorSet };
        vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, g_PipelineLayout, 0, 1, desc_set, 0, NULL);
    }

    // Bind Vertex And Index Buffer:
    {
        VkBuffer vertex_buffers[1] = { rb->VertexBuffer };
        VkDeviceSize vertex_offset[1] = { 0 };
        vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffers, vertex_offset);
        vkCmdBindIndexBuffer(command_buffer, rb->IndexBuffer, 0, sizeof(ImDrawIdx) == 2 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);
    }

    // Setup viewport:
    {
        VkViewport viewport;
        viewport.x = 0;
        viewport.y = 0;
        viewport.width = (float)fb_size.x;
        viewport.height = (float)fb_size.y;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(command_buffer, 0, 1, &viewport);
    }

    // Setup scale and translation:
    {
		float camera_aspect = float(fb_size.x) / float(fb_size.y);
		float world_aspect = float(world_size.x) / float(world_size.y);
		DrawUPC upc;
		upc.camera_from_world_shift = camera_from_world.xy();
		upc.camera_from_world_scale = vec2(world_aspect, 1.0f) * vec2(world_size.y) * scaleof(camera_from_world);
		upc.inv_camera_aspect = 1.0f / camera_aspect;
		upc.screen_from_grid_scale = float(fb_size.y) / float(world_size.y) * upc.camera_from_world_scale.y;
		upc.event_window_vis = vis.event_window_amt;
        vkCmdPushConstants(command_buffer, g_PipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(DrawUPC), &upc);
    }
}

void renderRecreatePipelineIfNeeded() {
	// if shaders changed
	if (g_Pipeline) return;

    VkResult err;
    VkShaderModule vert_module;
    VkShaderModule frag_module;

    // Create The Shader Modules:
    {
        VkShaderModuleCreateInfo vert_info = {};
        vert_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		vert_info.pCode = (uint32_t*)fileReadBinaryIntoMem("shaders/draw.vert.spv", &vert_info.codeSize);
        err = vkCreateShaderModule(evk.dev, &vert_info, evk.alloc, &vert_module);
        evkCheckError(err);
		free((void*)vert_info.pCode);
        VkShaderModuleCreateInfo frag_info = {};
        frag_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		frag_info.pCode = (uint32_t*)fileReadBinaryIntoMem("shaders/draw.frag.spv", &frag_info.codeSize);
        err = vkCreateShaderModule(evk.dev, &frag_info, evk.alloc, &frag_module);
        evkCheckError(err);
		free((void*)frag_info.pCode);
    }

	
    if (!g_Sampler)
    {
        VkSamplerCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        info.magFilter = VK_FILTER_NEAREST;
        info.minFilter = VK_FILTER_NEAREST;
        info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        info.minLod = -1000;
        info.maxLod = 1000;
        info.maxAnisotropy = 1.0f;
        err = vkCreateSampler(evk.dev, &info, evk.alloc, &g_Sampler);
        evkCheckError(err);
    }
	
	
    if (!g_DescriptorSetLayout)
    {
		VkDescriptorSetLayoutBinding setLayoutBindings[] = {
			evkMakeDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0),
			evkMakeDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1),
		};
		VkSampler color_sampler[1] = {g_Sampler};
		setLayoutBindings[0].pImmutableSamplers = color_sampler;
		VkSampler dev_sampler[1] = {g_Sampler};
		setLayoutBindings[1].pImmutableSamplers = dev_sampler;

		VkDescriptorSetLayoutCreateInfo descriptorLayout = evkMakeDescriptorSetLayoutCreateInfo(setLayoutBindings, ARRSIZE(setLayoutBindings));
		err = vkCreateDescriptorSetLayout(evk.dev, &descriptorLayout, evk.alloc, &g_DescriptorSetLayout);
		evkCheckError(err);
    }
	

	
    // Create Descriptor Set:
    {
        VkDescriptorSetAllocateInfo alloc_info = {};
        alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc_info.descriptorPool = evk.desc_pool;
        alloc_info.descriptorSetCount = 1;
        alloc_info.pSetLayouts = &g_DescriptorSetLayout;
        err = vkAllocateDescriptorSets(evk.dev, &alloc_info, &g_DescriptorSet);
        evkCheckError(err);
    }
	

    //if (!g_PipelineLayout)
    {
        VkPushConstantRange push_constants[1] = {};
        push_constants[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        push_constants[0].size = sizeof(DrawUPC);
        VkDescriptorSetLayout set_layout[1] = { g_DescriptorSetLayout };
        VkPipelineLayoutCreateInfo layout_info = {};
        layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout_info.setLayoutCount = 1;
        layout_info.pSetLayouts = set_layout;
        layout_info.pushConstantRangeCount = 1;
        layout_info.pPushConstantRanges = push_constants;
        err = vkCreatePipelineLayout(evk.dev, &layout_info, evk.alloc, &g_PipelineLayout);
        evkCheckError(err);
    }

    VkPipelineShaderStageCreateInfo stage[2] = {};
    stage[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stage[0].module = vert_module;
    stage[0].pName = "main";
    stage[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stage[1].module = frag_module;
    stage[1].pName = "main";

    VkVertexInputBindingDescription binding_desc[1] = {};
    binding_desc[0].stride = sizeof(DrawVert);
    binding_desc[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attribute_desc[2] = {};
    attribute_desc[0].location = 0;
    attribute_desc[0].binding = binding_desc[0].binding;
    attribute_desc[0].format = VK_FORMAT_R32G32_SFLOAT;
    attribute_desc[0].offset = IM_OFFSETOF(DrawVert, pos);
    attribute_desc[1].location = 1;
    attribute_desc[1].binding = binding_desc[0].binding;
    attribute_desc[1].format = VK_FORMAT_R32G32_SFLOAT;
    attribute_desc[1].offset = IM_OFFSETOF(DrawVert, uv);

    VkPipelineVertexInputStateCreateInfo vertex_info = {};
    vertex_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_info.vertexBindingDescriptionCount = ARRSIZE(binding_desc);
    vertex_info.pVertexBindingDescriptions = binding_desc;
    vertex_info.vertexAttributeDescriptionCount = ARRSIZE(attribute_desc);
    vertex_info.pVertexAttributeDescriptions = attribute_desc;

    VkPipelineInputAssemblyStateCreateInfo ia_info = {};
    ia_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewport_info = {};
    viewport_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_info.viewportCount = 1;
    viewport_info.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo raster_info = {};
    raster_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster_info.polygonMode = VK_POLYGON_MODE_FILL;
    raster_info.cullMode = VK_CULL_MODE_NONE;
    raster_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    raster_info.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms_info = {};
    ms_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState color_attachment[1] = {};
    color_attachment[0].blendEnable = VK_TRUE;
    color_attachment[0].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    color_attachment[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_attachment[0].colorBlendOp = VK_BLEND_OP_ADD;
    color_attachment[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_attachment[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_attachment[0].alphaBlendOp = VK_BLEND_OP_ADD;
    color_attachment[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineDepthStencilStateCreateInfo depth_info = {};
    depth_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;

    VkPipelineColorBlendStateCreateInfo blend_info = {};
    blend_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend_info.attachmentCount = 1;
    blend_info.pAttachments = color_attachment;

    VkDynamicState dynamic_states[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamic_state = {};
    dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.dynamicStateCount = (uint32_t)IM_ARRAYSIZE(dynamic_states);
    dynamic_state.pDynamicStates = dynamic_states;

    VkGraphicsPipelineCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    info.flags = g_PipelineCreateFlags;
    info.stageCount = 2;
    info.pStages = stage;
    info.pVertexInputState = &vertex_info;
    info.pInputAssemblyState = &ia_info;
    info.pViewportState = &viewport_info;
    info.pRasterizationState = &raster_info;
    info.pMultisampleState = &ms_info;
    info.pDepthStencilState = &depth_info;
    info.pColorBlendState = &blend_info;
    info.pDynamicState = &dynamic_state;
    info.layout = g_PipelineLayout;
    info.renderPass = evk.win.RenderPass;
    err = vkCreateGraphicsPipelines(evk.dev, evk.pipe_cache, 1, &info, evk.alloc, &g_Pipeline);
    evkCheckError(err);

    vkDestroyShaderModule(evk.dev, vert_module, evk.alloc);
    vkDestroyShaderModule(evk.dev, frag_module, evk.alloc);
}
VkDescriptorSet renderGetDescriptorSet() {
	return g_DescriptorSet;
}
VkSampler renderGetSampler() {
	return g_Sampler;
}
void renderDestroy() {
    destroyWindowRenderBuffers(&g_MainWindowRenderBuffers);
    if (g_Sampler)              { vkDestroySampler(evk.dev, g_Sampler, evk.alloc); g_Sampler = VK_NULL_HANDLE; }
    if (g_DescriptorSetLayout)  { vkDestroyDescriptorSetLayout(evk.dev, g_DescriptorSetLayout, evk.alloc); g_DescriptorSetLayout = VK_NULL_HANDLE; }
    if (g_PipelineLayout)       { vkDestroyPipelineLayout(evk.dev, g_PipelineLayout, evk.alloc); g_PipelineLayout = VK_NULL_HANDLE; }
    if (g_Pipeline)             { vkDestroyPipeline(evk.dev, g_Pipeline, evk.alloc); g_Pipeline = VK_NULL_HANDLE; }
}
void renderDraw(VkCommandBuffer command_buffer, ivec2 world_size, pose camera_from_world, RenderVis vis) {
    // Allocate array to store enough vertex/index buffers
    WindowRenderBuffers* wrb = &g_MainWindowRenderBuffers;
    if (wrb->Frames == NULL)
    {
        wrb->Index = 0;
        wrb->Count = evk.win.ImageCount;
        wrb->Frames = (FrameRenderBuffers*)malloc(sizeof(FrameRenderBuffers) * wrb->Count);
        memset(wrb->Frames, 0, sizeof(FrameRenderBuffers) * wrb->Count);
    }
    assert(wrb->Count == evk.win.ImageCount);
    wrb->Index = (wrb->Index + 1) % wrb->Count;
    FrameRenderBuffers* rb = &wrb->Frames[wrb->Index];

    VkResult err;

    // Create or resize the vertex/index buffers
    size_t vertex_size = sizeof(quad.verts);
    size_t index_size = sizeof(quad.idxs);
    if (rb->VertexBuffer == VK_NULL_HANDLE || rb->VertexBufferSize < vertex_size)
        createOrResizeVertexBuffer(rb->VertexBuffer, rb->VertexBufferMemory, rb->VertexBufferSize, vertex_size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    if (rb->IndexBuffer == VK_NULL_HANDLE || rb->IndexBufferSize < index_size)
        createOrResizeVertexBuffer(rb->IndexBuffer, rb->IndexBufferMemory, rb->IndexBufferSize, index_size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

    // Upload vertex/index data into a single contiguous GPU buffer
    {
        DrawVert* vtx_dst = NULL;
        DrawIdx* idx_dst = NULL;
        err = vkMapMemory(evk.dev, rb->VertexBufferMemory, 0, vertex_size, 0, (void**)(&vtx_dst));
        evkCheckError(err);
        err = vkMapMemory(evk.dev, rb->IndexBufferMemory, 0, index_size, 0, (void**)(&idx_dst));
        evkCheckError(err);
		memcpy(vtx_dst, quad.verts, sizeof(quad.verts));
		memcpy(idx_dst, quad.idxs, sizeof(quad.idxs));
        VkMappedMemoryRange range[2] = {};
        range[0].sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        range[0].memory = rb->VertexBufferMemory;
        range[0].size = VK_WHOLE_SIZE;
        range[1].sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        range[1].memory = rb->IndexBufferMemory;
        range[1].size = VK_WHOLE_SIZE;
        err = vkFlushMappedMemoryRanges(evk.dev, 2, range);
        evkCheckError(err);
        vkUnmapMemory(evk.dev, rb->VertexBufferMemory);
        vkUnmapMemory(evk.dev, rb->IndexBufferMemory);
    }

    // Setup desired Vulkan state
    setupRenderState(command_buffer, rb, ivec2(evk.win.Width, evk.win.Height), world_size, camera_from_world, vis);

    VkRect2D scissor;
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = evk.win.Width;
    scissor.extent.height = evk.win.Height;
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);

	vkCmdDrawIndexed(command_buffer, 6, 1, 0, 0, 0);
}