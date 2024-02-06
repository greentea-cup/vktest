#include "command.h"
#include "utils.h"

VkCommandPool create_command_pool(VkDevice device, uint32_t graphicsQFI) {
    VkCommandPoolCreateInfo cpCInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = VK_NULL_HANDLE,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = graphicsQFI};
    VkCommandPool commandPool;
    vkCreateCommandPool(device, &cpCInfo, VK_NULL_HANDLE, &commandPool);
    return commandPool;
}

VkCommandBuffer *create_command_buffers(VkDevice device, VkCommandPool commandPool, uint32_t bufferCount) {
    VkCommandBufferAllocateInfo cbAInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = VK_NULL_HANDLE,
        .commandPool = commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = bufferCount};
    ARR_ALLOC(VkCommandBuffer, buffers, bufferCount);
    vkAllocateCommandBuffers(device, &cbAInfo, buffers);
    return buffers;
}
