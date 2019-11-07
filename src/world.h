#pragma once
#include "core/vec2.h"
#include "wrap/evk.h"

template<VkFormat FORMAT>
struct StateMap {
	ivec2    size   = ivec2(3,8);

	// handles
	VkImage        image  = VK_NULL_HANDLE;
	VkDeviceMemory memory = VK_NULL_HANDLE;
	VkImageView    view   = VK_NULL_HANDLE;

	void destroy() {
		if (view)   { vkDestroyImageView(evk.dev, view, evk.alloc); view   = VK_NULL_HANDLE; }
		if (image)  { vkDestroyImage(evk.dev, image, evk.alloc);    image  = VK_NULL_HANDLE; }
		if (memory) { vkFreeMemory(evk.dev, memory, evk.alloc);     memory = VK_NULL_HANDLE; }
		size = ivec2(0,0);
	}
	bool resize(ivec2 new_size, VkCommandBuffer command_buffer) {
		if (new_size == size) return true; 
		destroy();
		size = new_size;
		if (size == ivec2(0,0)) return true; 

		VkResult err;

		// Create the Image:
		{
			VkImageCreateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			info.imageType = VK_IMAGE_TYPE_2D;
			info.format = FORMAT;
			info.flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
			info.extent.width = size.x;
			info.extent.height = size.y;
			info.extent.depth = 1;
			info.mipLevels = 1;
			info.arrayLayers = 1;
			info.samples = VK_SAMPLE_COUNT_1_BIT;
			info.tiling = VK_IMAGE_TILING_OPTIMAL;
			info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
			info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			err = vkCreateImage(evk.dev, &info, evk.alloc, &image);
			evkCheckError(err);
			if (err) { destroy(); return false; }
			VkMemoryRequirements req;
			vkGetImageMemoryRequirements(evk.dev, image, &req);
			VkMemoryAllocateInfo alloc_info = {};
			alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			alloc_info.allocationSize = req.size;
			alloc_info.memoryTypeIndex = evkMemoryType(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, req.memoryTypeBits);
			err = vkAllocateMemory(evk.dev, &alloc_info, evk.alloc, &memory);
			evkCheckError(err);
			if (err) { destroy(); return false; }
			err = vkBindImageMemory(evk.dev, image, memory, 0);
			evkCheckError(err);
			if (err) { destroy(); return false; }
			logInfo("IMAGE", "Resized to %dx%d, %d MiB (%d B)", size.x, size.y, alloc_info.allocationSize / (1024 * 1024), alloc_info.allocationSize);
		}

		// Create the View:
		{
			VkImageViewCreateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			info.image = image;
			info.viewType = VK_IMAGE_VIEW_TYPE_2D;
			info.format = FORMAT;
			info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			info.subresourceRange.levelCount = 1;
			info.subresourceRange.layerCount = 1;
			err = vkCreateImageView(evk.dev, &info, evk.alloc, &view);
			evkCheckError(err);
			if (err) { destroy(); return false; }
		}

		// Change format:
		{
		    VkImageMemoryBarrier use_barrier[1] = {};
			use_barrier[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			use_barrier[0].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			use_barrier[0].newLayout = VK_IMAGE_LAYOUT_GENERAL;
			use_barrier[0].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
			use_barrier[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			use_barrier[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			use_barrier[0].image = image;
			use_barrier[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			use_barrier[0].subresourceRange.levelCount = 1;
			use_barrier[0].subresourceRange.layerCount = 1;
			vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, use_barrier);
		}
		return true;
	}
};

struct World {
	StateMap<VK_FORMAT_R32G32B32A32_UINT> prng_state;
	StateMap<VK_FORMAT_R32G32B32A32_UINT> site_bits;
	StateMap<VK_FORMAT_R32_UINT> vote;
	StateMap<VK_FORMAT_R8G8B8A8_UINT> color;
	StateMap<VK_FORMAT_R32_UINT> event_count;
	StateMap<VK_FORMAT_R32G32B32A32_UINT> dev;

	ivec2 size = ivec2(0,0);

	VkImageView render_view = VK_NULL_HANDLE;

	void destroy();
	bool resize(ivec2 new_size, VkDescriptorSet update_descriptor_set, VkDescriptorSet draw_descriptor_set, VkSampler draw_sampler);
	ivec2 voteMapSize() const;
};