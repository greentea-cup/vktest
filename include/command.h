#ifndef COMMAND_H
#define COMMAND_H
#include "my_vulkan.h"
#include "vulkan/vulkan.h"

VkCommandPool create_command_pool(VkDevice device, uint32_t graphicsQFI);

VkCommandBuffer *create_command_buffers(
    VkDevice device, VkCommandPool commandPool, uint32_t swcImageCount);
#endif
