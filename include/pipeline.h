#ifndef PIPELINE_H
#define PIPELINE_H
#include "shader.h"
#include "vulkan/vulkan.h"

typedef struct {
    uint32_t bindingCount;
    VkVertexInputBindingDescription *bindings;
    uint32_t attributeCount;
    VkVertexInputAttributeDescription *attributes;
    VkPipelineRasterizationStateCreateInfo rasterizationParams;
    VkPipelineMultisampleStateCreateInfo multisampleParams;
    uint32_t colorBlendAttachmentCount;
    VkPipelineColorBlendAttachmentState *colorBlendAttachments;
    VkBool32 colorBlendLogicOpEnable;
    VkLogicOp colorBlendLogicOp;
    float colorBlendConstants[4];
} APipelineParams;

VkPipelineRasterizationStateCreateInfo APipeline_default_rasterizer();

VkPipelineMultisampleStateCreateInfo APipeline_default_multisampler();

VkPipelineColorBlendAttachmentState *APipeline_default_attachments(uint32_t *out_count);

APipelineParams APipeline_default();

void APipelineParams_free(APipelineParams);

VkPipelineLayout create_pipeline_layout(
    VkDevice device, uint32_t setLayoutCount, VkDescriptorSetLayout const *setLayouts,
    uint32_t pushConstantRangeCount, VkPushConstantRange const *pushConstantRanges);

VkPipeline create_pipeline(
    VkDevice device, VkPipelineLayout pipelineLayout, VkRenderPass renderPass,
    char const *entryPointGroup, uint32_t shaderCount, AShader const *shaders,
    APipelineParams args);

VkViewport make_viewport(VkExtent2D extent);

VkRect2D make_scissor(VkExtent2D extent, uint32_t left, uint32_t right, uint32_t up, uint32_t down);

#endif
