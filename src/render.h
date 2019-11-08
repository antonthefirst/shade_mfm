#pragma once
#include "wrap/evk.h"
#include "core/pose.h"

struct RenderVis {
	float event_window_amt = 0.0f;
};

void renderRecreatePipelineIfNeeded();
VkDescriptorSet renderGetDescriptorSet();
VkSampler       renderGetSampler();
void renderDestroy();
void renderDraw(VkCommandBuffer command_buffer, ivec2 world_size, pose camera_from_world, RenderVis vis);