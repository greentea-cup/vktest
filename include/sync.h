#ifndef SYNC_H
#define SYNC_H
#include "vulkan/vulkan.h"

VkSemaphore *create_semaphores(VkDevice device, uint32_t count);

VkFence *create_empty_fences(uint32_t count);

VkFence *create_fences(VkDevice device, uint32_t count);
#endif
