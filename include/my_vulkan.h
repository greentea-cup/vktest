#ifndef MY_VULKAN_H
#define MY_VULKAN_H
#include "vulkan/vulkan.h"
#include "SDL.h"

typedef enum {
    VIS_OK = 0, // Init successful
    VIS_CANNOT_CREATE_INSTANCE,
    VIS_CANNOT_CREATE_SURFACE,
    VIS_BAD_PHYSICAL_DEVICE, // suitable pysical device not found
    VIS_BAD_QUEUE_FAMILY, // suitable queue family not found
    VIS_CANNOT_CREATE_DEVICE,
    VIS_SURFACE_NOT_SUPPORTED,
    VIS_CANNOT_CREATE_SWAPCHAIN,
    VIS_CANNOT_CREATE_RENDER_PASS,
    VIS_CANNOT_CREATE_FRAMEBUFFER,
} VulkanInitStatus;

char const *VulkanInitStatus_str(VulkanInitStatus status);

typedef struct {
    VkShaderModule shaderModule;
    VkShaderStageFlagBits shaderStage;
} VulkanPipelineShaderStage;

typedef struct {
    VulkanInitStatus status;
    VkInstance ivk;
    VkPhysicalDevice pdevice;
    VkDevice device;
    uint32_t graphicsQFI;
    uint32_t presentQFI;
    uint32_t graphicsQMode;
    VkQueue drawQueue;
    VkQueue presentQueue;
    VkSurfaceKHR surface;
    VkFormat swcImageFormat;
    VkExtent2D swcExtent;
    VkSwapchainKHR swapchain;
    uint32_t swcImageCount;
    VkImage *swcImages; // count = swcImageCount
    VkImageView *swcImageViews; // count = swcImageCount
    VkRenderPass renderPass;
    VkFramebuffer *framebuffers; // count = swcImageCount
    uint32_t maxFrames;
    VkSemaphore *waitSemaphores; // count = maxFrames
    VkSemaphore *signalSemaphores; // count = maxFrames
    VkFence *frontFences; // count = maxFrames
    // VkFence *backFences; // count = swcImageCount
    VkCommandPool commandPool;
    VkCommandBuffer *commandBuffers; // count = maxFrames
    VkViewport viewport;
    VkRect2D scissor;
} Vulkan;

Vulkan init_vulkan(SDL_Window *window);

void destroy_vulkan(Vulkan *vulkan);

void acquire_next_image(Vulkan *vulkan);

int recreate_swapchain(Vulkan *vulkan);

void reset_command_pool(Vulkan *vulkan);

void update_viewport(Vulkan *vulkan);
#endif
