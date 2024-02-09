#include "sync.h"
#include "utils.h"

VkSemaphore *create_semaphores(VkDevice device, uint32_t count) {
    VkSemaphoreCreateInfo smCInfo = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    ARR_ALLOC(VkSemaphore, semaphores, count);
    for (uint32_t i = 0; i < count; i++) {
        vkCreateSemaphore(device, &smCInfo, NULL, semaphores + i);
    }
    return semaphores;
}

VkFence *create_empty_fences(uint32_t count) {
    ARR_ALLOC(VkFence, fences, count);
    for (uint32_t i = 0; i < count; i++) { fences[i] = NULL; }
    return fences;
}

VkFence *create_fences(VkDevice device, uint32_t count) {
    VkFenceCreateInfo fcCInfo = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,

        .flags = VK_FENCE_CREATE_SIGNALED_BIT};
    ARR_ALLOC(VkFence, fences, count);
    for (uint32_t i = 0; i < count; i++) { vkCreateFence(device, &fcCInfo, NULL, fences + i); }
    return fences;
}
