#include "sync.h"
#include "utils.h"

VkSemaphore *create_semaphores(VkDevice device, uint32_t count) {
    VkSemaphoreCreateInfo smCInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, .pNext = VK_NULL_HANDLE, .flags = 0};
    ARR_ALLOC(VkSemaphore, semaphores, count);
    for (uint32_t i = 0; i < count; i++) {
        vkCreateSemaphore(device, &smCInfo, VK_NULL_HANDLE, semaphores + i);
    }
    return semaphores;
}

VkFence *create_empty_fences(uint32_t count) {
    ARR_ALLOC(VkFence, fences, count);
    for (uint32_t i = 0; i < count; i++) { fences[i] = VK_NULL_HANDLE; }
    return fences;
}

VkFence *create_fences(VkDevice device, uint32_t count) {
    VkFenceCreateInfo fcCInfo = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = VK_NULL_HANDLE,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT};
    ARR_ALLOC(VkFence, fences, count);
    for (uint32_t i = 0; i < count; i++) {
        vkCreateFence(device, &fcCInfo, VK_NULL_HANDLE, fences + i);
    }
    return fences;
}
