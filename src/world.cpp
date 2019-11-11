#include "world.h"
#include "core/log.h"

#include "shaders/cpu_gpu_shared.inl"
#include "shaders/defines.inl" // for MFM world size

static ivec2 paddedSize(ivec2 world_size) {
	return world_size + ivec2(EVENT_WINDOW_RADIUS * 2 * 2);
}
void World::destroy() {
	VkImageView* views[] = {&color_draw_view, &dev_draw_view};
	for (int i = 0; i < ARRSIZE(views); ++i){
		if (*views[i]) {
			vkDestroyImageView(evk.dev, *views[i], evk.alloc);
			*views[i] = VK_NULL_HANDLE;
		}
	}
	 prng_state.destroy();
	  site_bits.destroy();
	       vote.destroy();
	      color.destroy();
	event_count.destroy();
	        dev.destroy();

	size = ivec2(0,0);
}
bool World::resize(ivec2 new_size, VkDescriptorSet update_descriptor_set, VkDescriptorSet draw_descriptor_set, VkSampler draw_sampler) {
	if (size == new_size) return true;

	VkResult err;

	// Wait until textures are no longer being used
	evkWaitUntilDeviceIdle();
	
	destroy();
	if (new_size == ivec2(0, 0)) return true;

	VkCommandPool command_pool = evk.win.Frames[evk.win.FrameIndex].CommandPool;
	VkCommandBuffer command_buffer = evk.win.Frames[evk.win.FrameIndex].CommandBuffer;
		
	evkResetCommandPool(command_pool);
	evkBeginCommandBuffer(command_buffer);

	if (  !site_bits.resize(new_size, command_buffer)) { destroy(); return false; }
	if (      !color.resize(new_size, command_buffer)) { destroy(); return false; }
	if (!event_count.resize(new_size, command_buffer)) { destroy(); return false; }
	if (        !dev.resize(new_size, command_buffer)) { destroy(); return false; }

	if ( !prng_state.resize(paddedSize(new_size), command_buffer)) { destroy(); return false; }
	if (       !vote.resize(paddedSize(new_size), command_buffer)) { destroy(); return false; }

	evkEndCommandBufferAndSubmit(command_buffer);

	size = new_size;

	// Update compute descriptor set:
	{
		VkImageView views[] = { prng_state.view, site_bits.view, vote.view, color.view, event_count.view, dev.view };
		VkWriteDescriptorSet write_desc[ARRSIZE(views)] = { };
		VkDescriptorImageInfo desc_image[ARRSIZE(views)][1] = { };
		for (int i = 0; i < ARRSIZE(views); ++i) {
			desc_image[i][0].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
			desc_image[i][0].imageView = views[i];
			write_desc[i].dstBinding = i;
			write_desc[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			write_desc[i].dstSet = update_descriptor_set;
			write_desc[i].descriptorCount = 1;
			write_desc[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			write_desc[i].pImageInfo = desc_image[i];
		}
		vkUpdateDescriptorSets(evk.dev, ARRSIZE(views), write_desc, 0, NULL);
	}

	VkImageView* draw_views[] = { &color_draw_view, &dev_draw_view };
	// Create render view:
	{
		
		VkImage images[] = { color.image, dev.image };
		VkFormat formats[] = { VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R32G32B32A32_UINT };
		for (int i = 0; i < ARRSIZE(draw_views); ++i) {
			VkImageViewCreateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			info.image = images[i];
			info.viewType = VK_IMAGE_VIEW_TYPE_2D;
			info.format = formats[i];
			info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			info.subresourceRange.levelCount = 1;
			info.subresourceRange.layerCount = 1;
			err = vkCreateImageView(evk.dev, &info, evk.alloc, draw_views[i]);
			evkCheckError(err);
			if (err) { destroy(); return false; }
		}
	}
	// Update render descriptor set:
	{
		/*
		VkDescriptorImageInfo desc_image[1] = {};
		desc_image[0].sampler = draw_sampler;
		desc_image[0].imageView = color_draw_view;
		desc_image[0].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		VkWriteDescriptorSet write_desc[1] = {};
		write_desc[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write_desc[0].dstSet = draw_descriptor_set;
		write_desc[0].descriptorCount = 1;
		write_desc[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		write_desc[0].pImageInfo = desc_image;
		vkUpdateDescriptorSets(evk.dev, 1, write_desc, 0, NULL);
		*/
		VkWriteDescriptorSet write_desc[ARRSIZE(draw_views)] = { };
		VkDescriptorImageInfo desc_image[ARRSIZE(draw_views)][1] = { };
		for (int i = 0; i < ARRSIZE(draw_views); ++i) {
			desc_image[i][0].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
			desc_image[i][0].imageView = *draw_views[i];
			desc_image[i][0].sampler = draw_sampler;
			write_desc[i].dstBinding = i;
			write_desc[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			write_desc[i].dstSet = draw_descriptor_set;
			write_desc[i].descriptorCount = 1;
			write_desc[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			write_desc[i].pImageInfo = desc_image[i];
		}
		vkUpdateDescriptorSets(evk.dev, ARRSIZE(draw_views), write_desc, 0, NULL);
	}

	// Wait until images are finished initializing before using them.
	evkWaitUntilDeviceIdle();

	return true;
}
ivec2 World::voteMapSize() const {
	return paddedSize(size);
}