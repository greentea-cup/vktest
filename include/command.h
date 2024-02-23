#ifndef COMMAND_H
#define COMMAND_H
#include "my_vulkan.h"
#include "vulkan/vulkan.h"

VkCommandPool A_create_command_pool(VkDevice device, uint32_t graphicsFamilyIndex);

VkCommandBuffer *A_create_command_buffers(
    VkDevice device, VkCommandPool commandPool, uint32_t swcImageCount);

VkCommandBuffer cmd_begin_one_time(VkDevice device, VkCommandPool commandPool);

void cmd_end_one_time(
    VkDevice device, VkCommandPool commandPool, VkQueue drawQueue, VkCommandBuffer cb);

typedef struct ACopyBufferParams {
    VkBuffer src;
    VkBuffer dst;
    VkDeviceSize srcOffset;
    VkDeviceSize dstOffset;
    VkDeviceSize size;
} ACopyBufferParams;
/*
 * 0 on success
 * 1 if buffer copy failed
 */
int copy_buffer(
    VkDevice device, VkCommandPool commandPool, VkQueue drawQueue, ACopyBufferParams args);

typedef struct ARecordCmdBuffersParams {
    VkBuffer vBuffer;
    VkBuffer iBuffer;
    uint32_t indexCount;
    uint32_t indexOffset;
    VkDescriptorSet *descriptorSets;
} ARecordCmdBuffersParams;

void record_command_buffer(
    VkRenderPass renderPass, VkFramebuffer const *framebuffers, VkExtent2D swapchainExtent,
    VkCommandBuffer const *commandBuffers, VkViewport viewport, VkRect2D scissor,
    VkPipeline pipeline, VkPipelineLayout plLayout, uint32_t currentFrame, uint32_t imageIndex,
    ARecordCmdBuffersParams args);

#endif
