#pragma once
#include "wrap/evk.h"
#include "core/pose.h"

void renderRecreatePipelineIfNeeded();
VkDescriptorSet renderGetDescriptorSet();
VkSampler       renderGetSampler();
void renderDestroy();
void renderDraw(VkCommandBuffer command_buffer, ivec2 world_size, pose camera_from_world);