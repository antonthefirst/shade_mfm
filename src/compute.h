#pragma once
#include "core/vec2.h"
#include "wrap/evk.h"

struct ComputeArgs {
	ivec2 site_info_idx = ivec2(-1);
};

bool computeRecreatePipelineIfNeeded();
VkDescriptorSet computeGetDescriptorSet();
void computeDestroy();
void computeBegin(VkCommandBuffer command_buffer, ComputeArgs args);
void computeStage(VkCommandBuffer command_buffer, int stage, ivec2 state_size);