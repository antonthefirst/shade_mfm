#pragma once
#include "wrap/evk.h"

void renderRecreatePipelineIfNeeded();
VkDescriptorSet renderGetDescriptorSet();
VkSampler       renderGetSampler();
void renderDestroy();
void renderDraw(VkCommandBuffer command_buffer);