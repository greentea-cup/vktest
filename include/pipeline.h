#ifndef PIPELINE_H
#define PIPELINE_H
#include "vulkan/vulkan.h"


VkViewport make_viewport(VkExtent2D extent);

VkRect2D make_scissor(VkExtent2D extent, uint32_t left, uint32_t right, uint32_t up, uint32_t down);

VkPipeline create_graphics_pipeline(VkDevice device, VkPipelineLayoutCreateInfo cInfo, VkShaderModule *shaderModules, VkShaderStageFlagBits *shaderStageFlags, uint32_t shaderModuleCount, VkRenderPass renderPass, VkPipelineLayout *out_pipelineLayout);
#endif
