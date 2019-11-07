#pragma once
#include "wrap/evk.h"

void computeRecreatePipelineIfNeeded();
VkDescriptorSet computeGetDescriptorSet();
void computeDestroy();
void computeBegin(VkCommandBuffer command_buffer);
void computeStage(VkCommandBuffer command_buffer, int stage, ivec2 state_size);