#include "command.h"
#include "utils.h"

VkCommandPool A_create_command_pool(VkDevice device, uint32_t graphicsFamilyIndex) {
    VkCommandPoolCreateInfo cpCInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,

        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = graphicsFamilyIndex};
    VkCommandPool commandPool;
    vkCreateCommandPool(device, &cpCInfo, NULL, &commandPool);
    return commandPool;
}

VkCommandBuffer *A_create_command_buffers(
    VkDevice device, VkCommandPool commandPool, uint32_t bufferCount) {
    VkCommandBufferAllocateInfo cbAInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,

        .commandPool = commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = bufferCount};
    ARR_ALLOC(VkCommandBuffer, buffers, bufferCount);
    vkAllocateCommandBuffers(device, &cbAInfo, buffers);
    return buffers;
}

VkCommandBuffer cmd_begin_one_time(VkDevice device, VkCommandPool commandPool) {
    VkCommandBufferAllocateInfo cbAInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandPool = commandPool,
        .commandBufferCount = 1};
    VkCommandBuffer cb;
    vkAllocateCommandBuffers(device, &cbAInfo, &cb);
    VkCommandBufferBeginInfo cbBInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
    vkBeginCommandBuffer(cb, &cbBInfo);
    return cb;
}

void cmd_end_one_time(
    VkDevice device, VkCommandPool commandPool, VkQueue drawQueue, VkCommandBuffer cb) {
    vkEndCommandBuffer(cb);
    VkSubmitInfo sInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &cb};
    vkQueueSubmit(drawQueue, 1, &sInfo, NULL);
    vkQueueWaitIdle(drawQueue);
    vkFreeCommandBuffers(device, commandPool, 1, &cb);
}

/*
 * 0 on success
 * 1 if buffer copy failed
 */
int copy_buffer(
    VkDevice device, VkCommandPool commandPool, VkQueue drawQueue, ACopyBufferParams args) {
    if (args.size == 0) return 0;

    VkCommandBuffer cb = cmd_begin_one_time(device, commandPool);

    VkBufferCopy copyRegion = {
        .srcOffset = args.srcOffset, .dstOffset = args.dstOffset, .size = args.size};
    vkCmdCopyBuffer(cb, args.src, args.dst, 1, &copyRegion);

    cmd_end_one_time(device, commandPool, drawQueue, cb);
    return 0;
}

void record_command_buffer(
    VkRenderPass renderPass, VkFramebuffer const *framebuffers, VkExtent2D swapchainExtent,
    VkCommandBuffer const *commandBuffers, VkViewport viewport, VkRect2D scissor,
    VkPipeline pipeline, VkPipelineLayout plLayout, uint32_t currentFrame, uint32_t imageIndex,
    ARecordCmdBuffersParams args) {
    VkCommandBufferBeginInfo cbBInfo = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    VkClearValue clearValue = {.color = {.float32 = {.325, .375, .75, 0.}}};
    VkRenderPassBeginInfo rpBInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = renderPass,
        .framebuffer = framebuffers[imageIndex],
        .renderArea = (VkRect2D){.offset = {0, 0}, .extent = swapchainExtent},
        .clearValueCount = 1,
        .pClearValues = &clearValue
    };
    VkCommandBuffer cmdBuf = commandBuffers[currentFrame];
    VkResult res = vkBeginCommandBuffer(cmdBuf, &cbBInfo);
    if (res != VK_SUCCESS) {
        eprintf(MSG_ERROR("vkBeginCommandBuffer %d"), res);
        return;
    }
    vkCmdBeginRenderPass(cmdBuf, &rpBInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkCmdSetViewport(cmdBuf, 0, 1, &viewport);
    vkCmdSetScissor(cmdBuf, 0, 1, &scissor);
    vkCmdBindVertexBuffers(cmdBuf, 0, 1, &args.vBuffer, (VkDeviceSize[]){0});
    vkCmdBindIndexBuffer(cmdBuf, args.iBuffer, 0, VulkanIndexType);
    vkCmdBindDescriptorSets(
        cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, plLayout, 0, 1, args.descriptorSets + currentFrame,
        0, NULL);
    vkCmdDrawIndexed(cmdBuf, args.indexCount, 1, args.indexOffset, 0, 0);
    vkCmdEndRenderPass(cmdBuf);
    res = vkEndCommandBuffer(cmdBuf);
    if (res != VK_SUCCESS) {
        eprintf(MSG_ERROR("vkEndCommandBuffer %d"), res);
        return;
    }
}
